using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Threading;
using Microsoft.VisualStudio.PlatformUI;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.Text.Tagging;
using Microsoft.VisualStudio.Threading;
using Microsoft.VisualStudio.Utilities;
using Newtonsoft.Json.Linq;

namespace Rutile.VisualStudio {
	[Export(typeof(IViewTaggerProvider)), ContentType(RutileContentType.Name), TagType(typeof(IntraTextAdornmentTag)), TextViewRole(PredefinedTextViewRoles.Document)]
	internal sealed class RutileTemplateContextProvider : IViewTaggerProvider {
		[Import]
		internal ITextDocumentFactoryService Documents = null!;

		public ITagger<T>? CreateTagger<T>(ITextView textView, ITextBuffer buffer) where T : ITag {
			if (!(textView is IWpfTextView view) || buffer != textView.TextBuffer) {
				return null;
			}
			return view.Properties.GetOrCreateSingletonProperty(() => new RutileTemplateContextTagger(view, Documents)) as ITagger<T>;
		}
	}

	internal sealed class RutileTemplateContextTagger : ITagger<IntraTextAdornmentTag>, IDisposable {
		private readonly IWpfTextView view;
		private readonly ITextDocumentFactoryService documents;
		private readonly DispatcherTimer timer;
		private readonly TemplateArgumentWidget widget;
		private TextBox arguments => widget.Arguments;
		private TextBlock status => widget.Status;
		private CancellationTokenSource? pending;
		private ITrackingPoint? anchor;
		private JObject? templatePosition;
		private bool disposed;

		internal RutileTemplateContextTagger(IWpfTextView view, ITextDocumentFactoryService documents) {
			this.view = view;
			this.documents = documents;
			widget = new TemplateArgumentWidget();
			widget.SetResourceReference(Control.BackgroundProperty, EnvironmentColors.ToolWindowBackgroundBrushKey);
			widget.SetResourceReference(Control.ForegroundProperty, EnvironmentColors.ToolWindowTextBrushKey);
			widget.Apply += () => Submit(false);
			widget.Clear += () => Submit(true);
			widget.Select += context => SelectExisting(context);
			timer = new DispatcherTimer(TimeSpan.FromMilliseconds(175), DispatcherPriority.Background, OnTimer, view.VisualElement.Dispatcher);
			timer.Stop();
			view.Caret.PositionChanged += OnCaret;
			view.TextBuffer.Changed += OnText;
			view.Closed += OnClosed;
			RutileEditorConnection.Instance.Changed += OnIndex;
			Schedule();
		}

		public event EventHandler<SnapshotSpanEventArgs>? TagsChanged;

		public IEnumerable<ITagSpan<IntraTextAdornmentTag>> GetTags(NormalizedSnapshotSpanCollection spans) {
			if (disposed || anchor == null || spans.Count == 0) {
				yield break;
			}
			var point = anchor.GetPoint(spans[0].Snapshot);
			var span = new SnapshotSpan(point, 0);
			if (spans.IntersectsWith(span)) {
				widget.Measure(new Size(double.PositiveInfinity, double.PositiveInfinity));
				yield return new TagSpan<IntraTextAdornmentTag>(span, new IntraTextAdornmentTag(widget, null, 0, widget.DesiredSize.Height * .75, widget.DesiredSize.Height, 0, PositionAffinity.Predecessor));
			}
		}

		private void Schedule() {
			if (!disposed) {
				timer.Stop();
				timer.Start();
			}
		}

		private void OnCaret(object sender, CaretPositionChangedEventArgs args) => Schedule();
		private void OnText(object sender, TextContentChangedEventArgs args) => Schedule();
		private void OnClosed(object sender, EventArgs args) => Dispose();

		private void OnIndex(object? sender, EventArgs args) {
			ThreadHelper.JoinableTaskFactory.RunAsync(async () => {
				await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
				Schedule();
			}).FileAndForget("Rutile/TemplateArgumentsRefresh");
		}

		private JObject? Position() {
			if (!documents.TryGetTextDocument(view.TextBuffer, out var document)) {
				return null;
			}
			var point = view.Caret.Position.BufferPosition;
			var line = point.GetContainingLine();
			return new JObject {
				["textDocument"] = new JObject { ["uri"] = new Uri(document.FilePath).AbsoluteUri },
				["position"] = new JObject { ["line"] = line.LineNumber, ["character"] = point.Position - line.Start.Position }
			};
		}

		private void OnTimer(object? sender, EventArgs args) {
			timer.Stop();
			ThreadHelper.JoinableTaskFactory.RunAsync(RefreshAsync).FileAndForget("Rutile/TemplateArgumentsRefresh");
		}

		private void Invalidate() {
			var snapshot = view.TextSnapshot;
			TagsChanged?.Invoke(this, new SnapshotSpanEventArgs(new SnapshotSpan(snapshot, 0, snapshot.Length)));
		}

		private async Task RefreshAsync() {
			pending?.Cancel();
			pending?.Dispose();
			var cancellation = pending = new CancellationTokenSource();
			var rpc = RutileEditorConnection.Instance.Rpc;
			var parameters = Position();
			if (rpc == null || parameters == null) {
				anchor = null;
				Invalidate();
				return;
			}
			var requestedSnapshot = view.TextSnapshot;
			try {
				var response = await rpc.InvokeWithParameterObjectAsync<JToken?>("rutile/templateContexts", parameters, cancellation.Token);
				if (disposed || cancellation.IsCancellationRequested) {
					return;
				}
				if (view.TextSnapshot != requestedSnapshot) {
					Schedule();
					return;
				}
				if (!(response is JObject result)) {
					return;
				}
				var declaration = TemplateContextResponse.Declaration(result, requestedSnapshot.Version.VersionNumber, view.TextSnapshot.Version.VersionNumber);
				if (!(declaration?["range"]?["start"] is JObject start)) {
					anchor = null;
					templatePosition = null;
					Invalidate();
					return;
				}
				var snapshot = view.TextSnapshot;
				var lineNumber = (int?)start["line"] ?? -1;
				if (lineNumber < 0 || lineNumber >= snapshot.LineCount) {
					return;
				}
				var line = snapshot.GetLineFromLineNumber(lineNumber);
				anchor = snapshot.CreateTrackingPoint(line.End, PointTrackingMode.Positive);
				templatePosition = parameters;
				widget.Update(declaration, result, (string?)parameters["textDocument"]?["uri"] + ":" + line.Start.Position);
				arguments.ToolTip = "Type template arguments: " + string.Join(", ", declaration["parameters"]?.ToObject<string[]>() ?? Array.Empty<string>());
				if (!arguments.IsKeyboardFocusWithin) {
					arguments.Text = (string?)result?["arguments"] ?? "";
				}
				var error = (string?)result?["error"] ?? "";
				status.Text = error.Length != 0 ? error : ((string?)result?["label"] ?? "Generic");
				status.ToolTip = error.Length != 0 ? error : status.Text;
				Invalidate();
			}
			catch (Exception error) when (error is OperationCanceledException || error is StreamJsonRpc.ConnectionLostException || error is StreamJsonRpc.RemoteInvocationException || error is ObjectDisposedException) {
				// An updated snapshot can supersede this request.
			}
		}

		private void SelectExisting(string context) {
			ThreadHelper.JoinableTaskFactory.RunAsync(async () => {
				var rpc = RutileEditorConnection.Instance.Rpc;
				if (rpc == null || templatePosition == null) {
					return;
				}
				var parameters = (JObject)templatePosition.DeepClone();
				parameters["context"] = context;
				await rpc.InvokeWithParameterObjectAsync<JToken?>("rutile/selectTemplateContext", parameters);
				Schedule();
			}).FileAndForget("Rutile/TemplateArgumentsSelect");
		}

		private void Submit(bool generic) {
			if (generic) {
				arguments.Text = "";
			}
			ThreadHelper.JoinableTaskFactory.RunAsync(async () => {
				var rpc = RutileEditorConnection.Instance.Rpc;
				if (rpc == null || templatePosition == null) {
					return;
				}
				var parameters = (JObject)templatePosition.DeepClone();
				parameters[generic ? "context" : "arguments"] = generic ? "" : arguments.Text;
				status.Text = generic ? "Generic" : "Checking arguments...";
				try {
					await rpc.InvokeWithParameterObjectAsync<JToken?>("rutile/selectTemplateContext", parameters);
					Schedule();
				}
				catch (Exception error) when (error is StreamJsonRpc.ConnectionLostException || error is StreamJsonRpc.RemoteInvocationException || error is ObjectDisposedException) {
					status.Text = error.Message;
				}
			}).FileAndForget("Rutile/TemplateArgumentsApply");
		}

		public void Dispose() {
			if (disposed) {
				return;
			}
			disposed = true;
			timer.Stop();
			pending?.Cancel();
			pending?.Dispose();
			view.Caret.PositionChanged -= OnCaret;
			view.TextBuffer.Changed -= OnText;
			view.Closed -= OnClosed;
			RutileEditorConnection.Instance.Changed -= OnIndex;
		}
	}
}
