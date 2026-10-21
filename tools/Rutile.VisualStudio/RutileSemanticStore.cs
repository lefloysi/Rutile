using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Text;
using Newtonsoft.Json.Linq;

namespace Rutile.VisualStudio {
	internal sealed class RutileSemanticStore : ISemanticColorSink {
		internal static RutileSemanticStore Instance { get; } = new RutileSemanticStore();
		private readonly object gate = new object();
		private readonly List<WeakReference<SemanticBufferColors>> buffers = new List<WeakReference<SemanticBufferColors>>();
		private string[] legend = Array.Empty<string>();
		private StreamJsonRpc.JsonRpc? connection;
		private readonly Dictionary<string, string> documents = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

		private RutileSemanticStore() {
			RutileEditorConnection.Instance.Changed += (_, __) => Refresh(null);
		}

		internal SemanticBufferColors Register(ITextBuffer buffer, string path) {
			var state = new SemanticBufferColors(buffer, new Uri(path).AbsoluteUri);
			lock (gate) {
				buffers.Add(new WeakReference<SemanticBufferColors>(state));
			}
			Refresh(state.Uri);
			return state;
		}

		internal async Task InitializeAsync() {
			var rpc = RutileEditorConnection.Instance.Rpc;
			if (rpc == null) {
				return;
			}

			var response = await rpc.InvokeWithParameterObjectAsync<JToken>("rutile/semanticTokensLegend", new JObject()).ConfigureAwait(false);
			if (!(response?["tokenTypes"] is JArray types)) {
				throw new InvalidOperationException("Kiln did not provide its semantic token legend.");
			}

			var names = new string[types.Count];
			for (var index = 0; index < types.Count; ++index) {
				names[index] = (string?)types[index] ?? string.Empty;
			}

			lock (gate) {
				if (!ReferenceEquals(rpc, RutileEditorConnection.Instance.Rpc)) {
					return;
				}
				connection = rpc;
				legend = names;
			}
			Refresh(null);
		}

		public void DocumentChanged(string method, JToken parameters) {
			var uri = (string?)parameters["textDocument"]?["uri"];
			if (uri == null) {
				return;
			}
			lock (gate) {
				if (method == "textDocument/didClose") {
					documents.Remove(uri);
				}
				else if (method == "textDocument/didOpen") {
					var text = (string?)parameters["textDocument"]?["text"];
					if (text != null) {
						documents[uri] = text;
					}
				}
				else if (method == "textDocument/didChange" && documents.TryGetValue(uri, out var previous)) {
					var text = RutileDocumentText.Apply(previous, parameters["contentChanges"] as JArray);
					if (text == null) {
						documents.Remove(uri);
					}
					else {
						documents[uri] = text;
					}
				}
			}
			if (method == "textDocument/didClose") {
				foreach (var buffer in Current(uri)) {
					buffer.Cancel();
				}
				return;
			}
			Refresh(uri);
		}

		private List<SemanticBufferColors> Current(string? uri) {
			var result = new List<SemanticBufferColors>();
			lock (gate) {
				for (var index = buffers.Count - 1; index >= 0; --index) {
					if (!buffers[index].TryGetTarget(out var state)) {
						buffers.RemoveAt(index);
						continue;
					}
					if (uri == null || string.Equals(uri, state.Uri, StringComparison.OrdinalIgnoreCase)) {
						result.Add(state);
					}
				}
			}
			return result;
		}

		private void Refresh(string? uri) {
			string[] names;
			StreamJsonRpc.JsonRpc? rpc;
			StreamJsonRpc.JsonRpc? previous;
			bool changed;
			lock (gate) {
				rpc = RutileEditorConnection.Instance.Rpc;
				previous = connection;
				changed = !ReferenceEquals(previous, rpc);
				if (changed) {
					connection = rpc;
					legend = Array.Empty<string>();
				}
				names = legend;
			}
			if (changed) {
				foreach (var buffer in Current(null)) {
					buffer.CancelConnection(previous);
				}
			}
			if (rpc == null || names.Length == 0) {
				return;
			}
			foreach (var buffer in Current(uri)) {
				string? text;
				lock (gate) {
					documents.TryGetValue(buffer.Uri, out text);
				}
				if (text != null) {
					buffer.Refresh(rpc, names, text);
				}
			}
		}

	}

	internal sealed class SemanticBufferColors {
		private readonly ITextBuffer buffer;
		private readonly SynchronizationContext? notifications;
		private readonly object gate = new object();
		private IReadOnlyList<ColoredSnapshotSpan> spans = Array.Empty<ColoredSnapshotSpan>();
		private CancellationTokenSource? request;
		private StreamJsonRpc.JsonRpc? requestConnection;
		private StreamJsonRpc.JsonRpc? publishedConnection;
		private bool published;
		internal string Uri { get; }
		internal event EventHandler? Changed;

		internal SemanticBufferColors(ITextBuffer buffer, string uri) {
			this.buffer = buffer;
			notifications = SynchronizationContext.Current;
			Uri = uri;
		}

		internal void Cancel() {
			lock (gate) {
				request?.Cancel();
			}
		}

		internal void CancelConnection(StreamJsonRpc.JsonRpc? previous) {
			lock (gate) {
				if (ReferenceEquals(requestConnection, previous)) {
					request?.Cancel();
				}
			}
			if (notifications != null) {
				notifications.Post(_ => Changed?.Invoke(this, EventArgs.Empty), null);
			}
			else {
				_ = ThreadHelper.JoinableTaskFactory.RunAsync(async () => {
					await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
					Changed?.Invoke(this, EventArgs.Empty);
				});
			}
		}

		internal void Translate(TextContentChangedEventArgs args) {
			lock (gate) {
				spans = Translate(spans, args.After);
			}
		}

		internal IReadOnlyList<ColoredSnapshotSpan> Current(ITextSnapshot snapshot, out bool authoritative) {
			lock (gate) {
				authoritative = published && publishedConnection != null && ReferenceEquals(publishedConnection, RutileEditorConnection.Instance.Rpc);
				return Translate(spans, snapshot);
			}
		}

		private static IReadOnlyList<ColoredSnapshotSpan> Translate(IReadOnlyList<ColoredSnapshotSpan> input, ITextSnapshot snapshot) {
			var result = new List<ColoredSnapshotSpan>(input.Count);
			foreach (var item in input) {
				var translated = item.Span.TranslateTo(snapshot, SpanTrackingMode.EdgeExclusive);
				// An edited identifier is unknown until the next compiler result. Other
				// bindings remain colored, including text shifted by edits above it.
				if (translated.Length != 0 && translated.GetText() == item.Span.GetText()) {
					result.Add(new ColoredSnapshotSpan(translated, item.Classification));
				}
			}
			return result;
		}

		internal void Refresh(StreamJsonRpc.JsonRpc rpc, string[] legend, string text) {
			var snapshot = buffer.CurrentSnapshot;
			if (!ReferenceEquals(rpc, RutileEditorConnection.Instance.Rpc) || snapshot.GetText() != text) {
				return;
			}
			CancellationTokenSource next;
			lock (gate) {
				if (!ReferenceEquals(rpc, RutileEditorConnection.Instance.Rpc)) {
					return;
				}
				request?.Cancel();
				next = new CancellationTokenSource();
				request = next;
				requestConnection = rpc;
			}
			_ = Task.Run(async () => {
				try {
					var parameters = new JObject { ["textDocument"] = new JObject { ["uri"] = Uri } };
					var response = await rpc.InvokeWithParameterObjectAsync<JToken?>("textDocument/semanticTokens/full", parameters, next.Token).ConfigureAwait(false);
					if (!(response is JObject result) || !(result["data"] is JArray data)) {
						return;
					}
					var decoded = RutileSemanticDecoder.Decode(snapshot.GetText(), data, legend);
					var prepared = new List<ColoredSnapshotSpan>(decoded.Count);
					foreach (var item in decoded) {
						prepared.Add(new ColoredSnapshotSpan(new SnapshotSpan(snapshot, item.Start, item.Length), item.Classification));
					}
					lock (gate) {
						if (next.IsCancellationRequested || !ReferenceEquals(request, next) || !ReferenceEquals(rpc, RutileEditorConnection.Instance.Rpc) || buffer.CurrentSnapshot.GetText() != snapshot.GetText()) {
							return;
						}
						spans = Translate(prepared, buffer.CurrentSnapshot);
						publishedConnection = rpc;
						published = true;
					}
					if (notifications == null) {
						await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync(next.Token);
						Changed?.Invoke(this, EventArgs.Empty);
					}
					else {
						notifications.Post(_ => Changed?.Invoke(this, EventArgs.Empty), null);
					}
				}
				catch (OperationCanceledException) {
				}
				catch (Exception error) {
					if (!next.IsCancellationRequested) {
						ServerOutput.Write("Semantic colors: " + error.Message);
					}
				}
				finally {
					lock (gate) {
						if (ReferenceEquals(request, next)) {
							request = null;
						}
						next.Dispose();
					}
				}
			});
		}
	}

	internal readonly struct ColoredSnapshotSpan {
		internal readonly SnapshotSpan Span;
		internal readonly string Classification;

		internal ColoredSnapshotSpan(SnapshotSpan span, string classification) {
			Span = span;
			Classification = classification;
		}
	}
}
