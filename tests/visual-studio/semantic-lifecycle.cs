using System;
using System.IO.Pipes;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio.LanguageServer.Client;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Classification;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using Rutile.VisualStudio;
using StreamJsonRpc;

internal sealed class SemanticReplies {
	internal JArray Types = new JArray("function");
	internal JArray Data = new JArray(0, 3, 1, 0, 0);
	internal TaskCompletionSource<bool>? Delay;
	internal readonly TaskCompletionSource<bool> Waiting = new TaskCompletionSource<bool>(TaskCreationOptions.RunContinuationsAsynchronously);
	internal TaskCompletionSource<bool> Completed = new TaskCompletionSource<bool>(TaskCreationOptions.RunContinuationsAsynchronously);
	internal int Legends;
	[JsonRpcMethod("rutile/semanticTokensLegend", UseSingleObjectParameterDeserialization = true)]
	public JObject Legend(JToken parameters) {
		Interlocked.Increment(ref Legends);
		return new JObject { ["tokenTypes"] = Types, ["tokenModifiers"] = new JArray() };
	}
	[JsonRpcMethod("textDocument/semanticTokens/full", UseSingleObjectParameterDeserialization = true)]
	public async Task<JObject> Tokens(JToken parameters) {
		var data = (JArray)Data.DeepClone();
		if (Delay != null) {
			Waiting.TrySetResult(true);
			await Delay.Task.ConfigureAwait(false);
		}
		Completed.TrySetResult(true);
		return new JObject { ["data"] = data };
	}
}

internal sealed class SemanticPeer : IDisposable {
	private readonly AnonymousPipeServerStream sending;
	private readonly AnonymousPipeClientStream receiving;
	private readonly AnonymousPipeServerStream peerSending;
	private readonly AnonymousPipeClientStream peerReceiving;
	private readonly JsonRpc remote;
	internal readonly JsonRpc Rpc;
	internal SemanticPeer(SemanticReplies replies) {
		sending = new AnonymousPipeServerStream(PipeDirection.Out, System.IO.HandleInheritability.None);
		peerReceiving = new AnonymousPipeClientStream(PipeDirection.In, sending.ClientSafePipeHandle);
		peerSending = new AnonymousPipeServerStream(PipeDirection.Out, System.IO.HandleInheritability.None);
		receiving = new AnonymousPipeClientStream(PipeDirection.In, peerSending.ClientSafePipeHandle);
		remote = new JsonRpc(new HeaderDelimitedMessageHandler(peerSending, peerReceiving));
		remote.AddLocalRpcTarget(replies);
		Rpc = new JsonRpc(new HeaderDelimitedMessageHandler(sending, receiving));
		remote.StartListening();
		Rpc.StartListening();
	}
	public void Dispose() {
		Rpc.Dispose();
		remote.Dispose();
		sending.Dispose();
		receiving.Dispose();
		peerSending.Dispose();
		peerReceiving.Dispose();
	}
}

public static partial class EditorObservations {
	private static async Task Until(Func<bool> ready, CancellationToken token) {
		while (!ready()) {
			await Task.Delay(10, token).ConfigureAwait(false);
		}
	}

	public static string SemanticLifecycle() {
		return ObserveSemanticLifecycleAsync().GetAwaiter().GetResult();
	}

	private static async Task<string> ObserveSemanticLifecycleAsync() {
		using (var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(15))) {
			var text = "\"f\" fn g();";
			ITextBuffer? buffer = null;
			ITextSnapshot? snapshot = null;
			buffer = new EditorInterface<ITextBuffer>((method, values) => method.Name == "get_CurrentSnapshot" ? snapshot : null).Value;
			snapshot = new EditorInterface<ITextSnapshot>((method, values) => {
				if (method.Name == "get_Length") {
					return text.Length;
				}
				if (method.Name == "get_TextBuffer") {
					return buffer;
				}
				if (method.Name == "GetText") {
					return values.Length == 0 ? text : values.Length == 1
						? text.Substring(((Span)values[0]).Start, ((Span)values[0]).Length)
						: text.Substring((int)values[0], (int)values[1]);
				}
				return null;
			}).Value;
			var uri = new Uri(System.IO.Path.Combine(System.IO.Path.GetTempPath(), "rutile-editor-state.rt")).AbsoluteUri;
			var previousContext = SynchronizationContext.Current;
			SynchronizationContext.SetSynchronizationContext(new SynchronizationContext());
			var state = RutileSemanticStore.Instance.Register(buffer, new Uri(uri).LocalPath);
			SynchronizationContext.SetSynchronizationContext(previousContext);
			var document = new JObject { ["textDocument"] = new JObject { ["uri"] = uri, ["text"] = text } };
			RutileSemanticStore.Instance.DocumentChanged("textDocument/didOpen", document);
			var replies = new SemanticReplies { Data = new JArray(0, 7, 1, 0, 0) };
			using (var first = new SemanticPeer(replies)) {
				var client = new RutileLanguageClient();
				await client.OnServerInitializedAsync().ConfigureAwait(false);
				await ((ILanguageClientCustomMessage2)client).AttachForCustomMessageAsync(first.Rpc).ConfigureAwait(false);
				await Until(() => state.Current(snapshot, out _).Any(span => span.Classification == "rutile.function"), timeout.Token).ConfigureAwait(false);
				var initial = state.Current(snapshot, out _).Select(span => span.Classification).ToArray();
				replies.Delay = new TaskCompletionSource<bool>(TaskCreationOptions.RunContinuationsAsynchronously);
				replies.Completed = new TaskCompletionSource<bool>(TaskCreationOptions.RunContinuationsAsynchronously);
				replies.Data = new JArray(0, 4, 2, 0, 0);
				RutileSemanticStore.Instance.DocumentChanged("textDocument/didSave", document);
				await Until(() => replies.Waiting.Task.IsCompleted, timeout.Token).ConfigureAwait(false);
				var replacement = new SemanticReplies { Types = new JArray("keyword", "function"), Data = new JArray(0, 4, 2, 0, 0, 0, 3, 1, 1, 0) };
				using (var second = new SemanticPeer(replacement)) {
					await ((ILanguageClientCustomMessage2)client).AttachForCustomMessageAsync(second.Rpc).ConfigureAwait(false);
					await Until(() => state.Current(snapshot, out _).Any(span => span.Classification == "rutile.keyword"), timeout.Token).ConfigureAwait(false);
					replies.Delay.TrySetResult(true);
					await Until(() => replies.Completed.Task.IsCompleted, timeout.Token).ConfigureAwait(false);
					await Task.Delay(100, timeout.Token).ConfigureAwait(false);
					var afterLateReply = state.Current(snapshot, out _).Select(span => span.Classification).ToArray();
					var disconnected = new TaskCompletionSource<bool>(TaskCreationOptions.RunContinuationsAsynchronously);
					first.Rpc.Disconnected += (_, __) => disconnected.TrySetResult(true);
					first.Rpc.Dispose();
					await Until(() => disconnected.Task.IsCompleted, timeout.Token).ConfigureAwait(false);
					var replacementRetained = ReferenceEquals(RutileEditorConnection.Instance.Rpc, second.Rpc);
					replacement.Data = new JArray(0, 1, 1, 1, 0);
					RutileSemanticStore.Instance.DocumentChanged("textDocument/didSave", document);
					await Until(() => state.Current(snapshot, out _).Any(span => span.Span.Start.Position == 1 && span.Classification == "rutile.function"), timeout.Token).ConfigureAwait(false);
					var registry = new EditorInterface<IClassificationTypeRegistryService>((method, values) => method.Name == "GetClassificationType"
						? new EditorInterface<IClassificationType>((member, arguments) => member.Name == "get_Classification" ? values[0] : null).Value : null).Value;
					var classifier = new RutileLexicalClassifier(buffer, registry, state);
					var changed = classifier.GetClassificationSpans(new SnapshotSpan(snapshot, 0, text.Length)).Select(span => span.ClassificationType.Classification).ToArray();
					RutileSemanticStore.Instance.DocumentChanged("textDocument/didClose", document);
					return JsonConvert.SerializeObject(new { initial, after_late_reply = afterLateReply, replacement_retained = replacementRetained,
						changed, initial_legends = replies.Legends, replacement_legends = replacement.Legends });
				}
			}
		}
	}
}
