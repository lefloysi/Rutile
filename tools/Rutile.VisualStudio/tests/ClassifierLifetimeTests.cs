using System;
using System.Linq;
using System.Reflection;
using System.IO;
using System.Collections.Concurrent;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio.LanguageServer.Client;
using StreamJsonRpc;
using System.Runtime.Remoting.Messaging;
using System.Runtime.Remoting.Proxies;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Classification;
using Microsoft.VisualStudio.Utilities;
using Newtonsoft.Json.Linq;

sealed class InterfaceStub<T> : RealProxy {
	private readonly Func<MethodInfo, object[], object> invoke;

	internal InterfaceStub(Func<MethodInfo, object[], object> invoke) : base(typeof(T)) {
		this.invoke = invoke;
	}

	internal T Value {
		get {
			return (T)GetTransparentProxy();
		}
	}

	public override IMessage Invoke(IMessage message) {
		var call = (IMethodCallMessage)message;
		try {
			var result = invoke((MethodInfo)call.MethodBase, call.Args);
			return new ReturnMessage(result, call.Args, call.Args.Length, call.LogicalCallContext, call);
		}
		catch (Exception error) {
			return new ReturnMessage(error, call);
		}
	}
}

static class ClassifierLifetimeTests {
	private static void BraceCompletion(Assembly assembly, ITextSnapshot snapshot) {
		var providerType = assembly.GetType("Rutile.VisualStudio.RutileBraceCompletionProvider", true);
		var pairs = providerType.GetCustomAttributesData().Where(attribute => attribute.AttributeType.Name == "BracePairAttribute").Select(attribute => "" + (char)attribute.ConstructorArguments[0].Value + (char)attribute.ConstructorArguments[1].Value).ToArray();
		if (!pairs.OrderBy(pair => pair).SequenceEqual(new[] { "()", "[]", "{}", "\"\"", "''" }.OrderBy(pair => pair))) {
			throw new Exception("Native brace completion does not export the expected punctuation pairs");
		}
		var defaultsType = assembly.GetType("Rutile.VisualStudio.RutileBraceCompletionDefaults", true);
		var defaultPairs = defaultsType.GetCustomAttributesData().Where(attribute => attribute.AttributeType.Name == "BracePairAttribute").Select(attribute => "" + (char)attribute.ConstructorArguments[0].Value + (char)attribute.ConstructorArguments[1].Value);
		if (!defaultsType.GetInterfaces().Any(type => type.FullName == "Microsoft.VisualStudio.Text.BraceCompletion.IBraceCompletionDefaultProvider") || !defaultPairs.OrderBy(pair => pair).SequenceEqual(pairs.OrderBy(pair => pair))) {
			throw new Exception("Native default brace session provider does not match the context provider");
		}
		var canStart = providerType.GetMethod("CanStart", BindingFlags.NonPublic | BindingFlags.Static);
		foreach (var prefix in new[] { "", "fn f", "/* closed */", "// comment\n", "\"closed\"", "'x'" }) {
			if (!(bool)canStart.Invoke(null, new object[] { prefix, '(', ')' })) {
				throw new Exception("Brace completion rejected code after closed trivia: " + prefix);
			}
		}
		foreach (var prefix in new[] { "// comment", "/* comment", "\"literal", "'literal", "\"escaped\\\"" }) {
			if ((bool)canStart.Invoke(null, new object[] { prefix, '(', ')' })) {
				throw new Exception("Brace completion entered comment or literal: " + prefix);
			}
		}
		if ((bool)canStart.Invoke(null, new object[] { "template", '<', '>' })) {
			throw new Exception("Brace completion guessed whether a comparison was a template");
		}
		var provider = Activator.CreateInstance(providerType, true);
		var classifiers = new InterfaceStub<IClassifierAggregatorService>((method, args) => new InterfaceStub<IClassifier>((member, values) => Array.Empty<ClassificationSpan>()).Value).Value;
		providerType.GetField("Classifiers", BindingFlags.NonPublic | BindingFlags.Instance).SetValue(provider, classifiers);
		var arguments = new object[] { null, new SnapshotPoint(snapshot, 4), '(', ')', null };
		if (!(bool)providerType.GetMethod("TryCreateContext").Invoke(provider, arguments) || arguments[4] == null) {
			throw new Exception("Actual SDK brace provider did not create the native editor context");
		}
		var contextType = assembly.GetType("Rutile.VisualStudio.RutileBraceCompletionContext", true);
		var indent = contextType.GetMethod("BodyIndent", BindingFlags.NonPublic | BindingFlags.Static);
		if ((string)indent.Invoke(null, new object[] { "\t", 4, 4, false }) != "\t\t" || (string)indent.Invoke(null, new object[] { "  ", 2, 4, true }) != "    " || (string)indent.Invoke(null, new object[] { "", 4, 8, false }) != "    ") {
			throw new Exception("Brace newline indentation does not respect editor indent and tab settings");
		}
		Console.WriteLine("Native brace MEF pairs, context suppression and editor indentation settings PASS");
	}

	private static System.Collections.Generic.IList<ClassificationSpan> Colors(object classifier, ITextSnapshot snapshot, int length) {
		return ((IClassifier)classifier).GetClassificationSpans(new SnapshotSpan(snapshot, 0, length));
	}

	private static void WaitUntil(Func<bool> condition, string message) {
		var deadline = DateTime.UtcNow.AddSeconds(5);
		while (!condition()) {
			if (DateTime.UtcNow >= deadline) {
				throw new Exception(message);
			}
			Thread.Sleep(10);
		}
	}

	static int Main(string[] args) {
		SynchronizationContext.SetSynchronizationContext(new SynchronizationContext());
		var properties = new PropertyCollection();
		var source = "fn f();";
		ITextSnapshot snapshot = null;
		ITextBuffer buffer = null;
		buffer = new InterfaceStub<ITextBuffer>((method, values) => method.Name == "get_Properties" ? (object)properties : method.Name == "get_CurrentSnapshot" ? snapshot : null).Value;
		snapshot = new InterfaceStub<ITextSnapshot>((method, values) => {
			if (method.Name == "get_Length") {
				return source.Length;
			}

			if (method.Name == "get_TextBuffer") {
				return buffer;
			}

			if (method.Name == "GetText") {
				if (values.Length == 0) {
					return source;
				}
				if (values.Length == 1) {
					var span = (Span)values[0];
					return source.Substring(span.Start, span.Length);
				}
				return source.Substring((int)values[0], (int)values[1]);
			}

			return null;
		}).Value;

		var document = new InterfaceStub<ITextDocument>((method, values) => method.Name == "get_TextBuffer" ? (object)buffer : method.Name == "get_FilePath" ? @"C:\fixture\late.rt" : null).Value;
		EventHandler<TextDocumentEventArgs> created = null;
		var available = false;
		var factory = new InterfaceStub<ITextDocumentFactoryService>((method, values) => {
			if (method.Name == "TryGetTextDocument") {
				values[1] = available ? document : null;
				return available;
			}

			if (method.Name == "add_TextDocumentCreated") {
				created += (EventHandler<TextDocumentEventArgs>)values[0];
			}

			if (method.Name == "remove_TextDocumentCreated") {
				created -= (EventHandler<TextDocumentEventArgs>)values[0];
			}

			return null;
		}).Value;

		var assembly = Assembly.LoadFrom(args[0]);
		BraceCompletion(assembly, snapshot);
		var providerType = assembly.GetType("Rutile.VisualStudio.RutileLexicalClassifierProvider", true);
		var provider = Activator.CreateInstance(providerType, true);
		providerType.GetField("Documents", BindingFlags.NonPublic | BindingFlags.Instance).SetValue(provider, factory);
		var registry = new InterfaceStub<IClassificationTypeRegistryService>((method, values) => {
			if (method.Name == "GetClassificationType") {
				string name = (string)values[0];
				return new InterfaceStub<IClassificationType>((member, arguments) => member.Name == "get_Classification" ? (object)name : null).Value;
			}

			return null;
		}).Value;
		providerType.GetField("Types", BindingFlags.NonPublic | BindingFlags.Instance).SetValue(provider, registry);

		var classifier = providerType.GetMethod("GetClassifier").Invoke(provider, new object[] { buffer });
		var semantic = classifier.GetType().GetField("semantic", BindingFlags.NonPublic | BindingFlags.Instance);
		if (semantic.GetValue(classifier) != null) {
			throw new Exception("Fixture expected documentless buffer initially");
		}

		var otherBuffer = new InterfaceStub<ITextBuffer>((method, values) => null).Value;
		var otherDocument = new InterfaceStub<ITextDocument>((method, values) => method.Name == "get_TextBuffer" ? (object)otherBuffer : method.Name == "get_FilePath" ? @"C:\fixture\other.rt" : null).Value;
		created?.Invoke(null, new TextDocumentEventArgs(otherDocument));
		if (semantic.GetValue(classifier) != null) {
			throw new Exception("Unrelated document attached to the wrong buffer");
		}

		available = true;
		created?.Invoke(null, new TextDocumentEventArgs(document));
		if (semantic.GetValue(classifier) == null) {
			throw new Exception("Compiler semantic colors never attached after TextDocumentCreated");
		}

		var retained = semantic.GetValue(classifier);
		var uri = (string)retained.GetType().GetProperty("Uri", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(retained);
		if (uri != new Uri(@"C:\fixture\late.rt").AbsoluteUri) {
			throw new Exception("Semantic requests use the wrong document URI: " + uri);
		}

		created?.Invoke(null, new TextDocumentEventArgs(document));
		if (!ReferenceEquals(retained, semantic.GetValue(classifier))) {
			throw new Exception("Repeated document notification replaced the semantic color owner");
		}

		using (var remote = new RpcStream())
		using (var socket = new RpcStream()) {
			remote.Peer = socket;
			socket.Peer = remote;
			using (var serverRpc = new JsonRpc(remote))
			using (var clientRpc = new JsonRpc(socket)) {
				var target = new SemanticServer();
				serverRpc.AddLocalRpcTarget(target);
				serverRpc.StartListening();
				clientRpc.StartListening();
				var languageClient = (ILanguageClient)Activator.CreateInstance(assembly.GetType("Rutile.VisualStudio.RutileLanguageClient", true), new object[] { new InterfaceStub<Kiln.VisualStudio.IKilnWorkspace>((method, values) => null).Value });
				if (!((JObject)languageClient.InitializationOptions).Value<bool>("rutileSemanticColors")) {
					throw new Exception("Custom classifier did not opt out of native semantic tagger");
				}
				var initializedFirst = args.Length > 1 && args[1] == "initialized-first";
				if (initializedFirst) {
					languageClient.OnServerInitializedAsync().GetAwaiter().GetResult();
				}
				((ILanguageClientCustomMessage2)languageClient).AttachForCustomMessageAsync(clientRpc).GetAwaiter().GetResult();
				var middleLayer = (ILanguageClientMiddleLayer)((ILanguageClientCustomMessage2)languageClient).MiddleLayer;
				if (middleLayer.CanHandle("initialize")) {
					throw new Exception("Legend bootstrap still depends on unsupported initialize middleware");
				}
				var opened = new JObject { ["textDocument"] = new JObject { ["uri"] = uri, ["text"] = source } };
				middleLayer.HandleNotificationAsync("textDocument/didOpen", opened, parameters => clientRpc.NotifyWithParameterObjectAsync("textDocument/didOpen", parameters)).GetAwaiter().GetResult();
				if (!initializedFirst) {
					languageClient.OnServerInitializedAsync().GetAwaiter().GetResult();
				}
				var deadline = DateTime.UtcNow.AddSeconds(5);
				while (!((IClassifier)classifier).GetClassificationSpans(new SnapshotSpan(snapshot, 0, source.Length)).Any(item => item.Span.Start.Position == 3 && item.ClassificationType.Classification == "rutile.function")) {
					if (DateTime.UtcNow >= deadline) {
						throw new Exception("Actual semantic RPC did not deliver function classification; requests=" + target.Requests);
					}
					Thread.Sleep(10);
				}
				if (!target.Opened || target.Legends != 1 || target.Requests == 0) {
					throw new Exception("Public client lifecycle did not synchronize document, fetch legend and request tokens");
				}
				target.Delay = new TaskCompletionSource<bool>();
				target.Data = new JArray(0, 0, 2, 0, 0);
				middleLayer.HandleNotificationAsync("textDocument/didSave", opened, _ => Task.CompletedTask).GetAwaiter().GetResult();
				WaitUntil(() => target.Waiting, "Old server did not receive delayed color request");
				using (var newRemote = new RpcStream())
				using (var newSocket = new RpcStream()) {
					newRemote.Peer = newSocket;
					newSocket.Peer = newRemote;
					using (var newServer = new JsonRpc(newRemote))
					using (var newClient = new JsonRpc(newSocket)) {
						var replacement = new SemanticServer { Types = new JArray("keyword", "function"), Data = new JArray(0, 0, 2, 0, 0, 0, 3, 1, 1, 0) };
						newServer.AddLocalRpcTarget(replacement);
						newServer.StartListening();
						newClient.StartListening();
						((ILanguageClientCustomMessage2)languageClient).AttachForCustomMessageAsync(newClient).GetAwaiter().GetResult();
						WaitUntil(() => Colors(classifier, snapshot, source.Length).Any(item => item.Span.Start.Position == 0 && item.ClassificationType.Classification == "rutile.keyword"), "Replacement legend did not repaint existing buffer");
						if (replacement.Legends != 1 || replacement.Requests == 0 || !Colors(classifier, snapshot, source.Length).Any(item => item.Span.Start.Position == 3 && item.ClassificationType.Classification == "rutile.function")) {
							throw new Exception("Reconnect did not use replacement's reordered legend");
						}
						target.Delay.SetResult(true);
						WaitUntil(() => target.Completed, "Old delayed response never completed");
						Thread.Sleep(100);
						if (Colors(classifier, snapshot, source.Length).Any(item => item.Span.Start.Position == 0 && item.ClassificationType.Classification == "rutile.function")) {
							throw new Exception("Late old server response replaced new session colors");
						}
						var oldDisconnected = new TaskCompletionSource<bool>();
						clientRpc.Disconnected += (_, __) => oldDisconnected.TrySetResult(true);
						clientRpc.Dispose();
						WaitUntil(() => oldDisconnected.Task.IsCompleted, "Old RPC disconnect notification did not complete");
						var editorType = assembly.GetType("Rutile.VisualStudio.RutileEditorConnection", true);
						var editor = editorType.GetProperty("Instance", BindingFlags.Static | BindingFlags.NonPublic).GetValue(null);
						var activeRpc = editorType.GetProperty("Rpc", BindingFlags.Instance | BindingFlags.NonPublic).GetValue(editor);
						if (!ReferenceEquals(activeRpc, newClient)) {
							throw new Exception("Old RPC disconnect cleared the replacement connection");
						}
						source = "\"f\"    ";
						classifier.GetType().GetField("cache", BindingFlags.NonPublic | BindingFlags.Instance).SetValue(classifier, null);
						replacement.Data = new JArray(0, 1, 1, 1, 0);
						var edit = new JObject { ["textDocument"] = new JObject { ["uri"] = uri }, ["contentChanges"] = new JArray(new JObject { ["text"] = source }) };
						middleLayer.HandleNotificationAsync("textDocument/didChange", edit, _ => Task.CompletedTask).GetAwaiter().GetResult();
						WaitUntil(() => Colors(classifier, snapshot, source.Length).Any(item => item.Span.Start.Position == 1 && item.ClassificationType.Classification == "rutile.function"), "Local string lexer masked authoritative server token");
						if (Colors(classifier, snapshot, source.Length).Any(item => item.ClassificationType.Classification == "string")) {
							throw new Exception("Local string classification overrode connected server result");
						}
					}
				}

			}

		}

		var actual = Colors(classifier, snapshot, source.Length);
		if (!actual.Any(item => item.Span.Start.Position == 1 && item.ClassificationType.Classification == "rutile.function") || actual.Any(item => item.ClassificationType.Classification == "string")) {
			throw new Exception("Late-attached compiler function binding was not returned as a function classification");
		}

		var format = Activator.CreateInstance(assembly.GetType("Rutile.VisualStudio.RutileFunctionFormat", true), true);
		var color = format.GetType().GetProperty("ForegroundColor").GetValue(format);
		if (color.ToString() != "#FFDCDCAA") {
			throw new Exception("Function format is not yellow: " + color);
		}

		Console.WriteLine("Client init orders, server-owned colors, reordered reconnect legend and stale reply rejection PASS");
		return 0;
	}
}

sealed class SemanticServer {
	internal bool Opened;
	internal int Legends;
	internal int Requests;
	internal JArray Types = new JArray("function");
	internal JArray Data = new JArray(0, 3, 1, 0, 0);
	internal TaskCompletionSource<bool> Delay;
	internal volatile bool Waiting;
	internal volatile bool Completed;

	[JsonRpcMethod("textDocument/didOpen", UseSingleObjectParameterDeserialization = true)]
	public void Open(JToken parameters) {
		Opened = (string)parameters["textDocument"]["text"] == "fn f();";
	}

	[JsonRpcMethod("rutile/semanticTokensLegend", UseSingleObjectParameterDeserialization = true)]
	public JObject Legend(JToken parameters) {
		Interlocked.Increment(ref Legends);
		return new JObject { ["tokenTypes"] = Types, ["tokenModifiers"] = new JArray() };
	}

	[JsonRpcMethod("textDocument/semanticTokens/full", UseSingleObjectParameterDeserialization = true)]
	public async Task<JObject> Tokens(JToken parameters) {
		Interlocked.Increment(ref Requests);
		var data = Data;
		var delay = Delay;
		if (delay != null) {
			Waiting = true;
			await delay.Task.ConfigureAwait(false);
			Completed = true;
		}
		return new JObject { ["data"] = data.DeepClone() };
	}
}

sealed class RpcStream : Stream {
	internal RpcStream Peer;
	private readonly BlockingCollection<byte[]> incoming = new BlockingCollection<byte[]>();
	private byte[] current = Array.Empty<byte>();
	private int offset;

	public override bool CanRead => true;
	public override bool CanWrite => true;
	public override bool CanSeek => false;
	public override long Length => throw new NotSupportedException();
	public override long Position { get => throw new NotSupportedException(); set => throw new NotSupportedException(); }

	public override int Read(byte[] buffer, int begin, int count) {
		if (offset == current.Length) {
			try {
				current = incoming.Take();
				offset = 0;
			}
			catch (InvalidOperationException) {
				return 0;
			}
		}
		var copied = Math.Min(count, current.Length - offset);
		Array.Copy(current, offset, buffer, begin, copied);
		offset += copied;
		return copied;
	}

	public override Task<int> ReadAsync(byte[] buffer, int begin, int count, CancellationToken token) {
		return Task.Run(() => Read(buffer, begin, count), token);
	}

	public override void Write(byte[] buffer, int begin, int count) {
		var copy = new byte[count];
		Array.Copy(buffer, begin, copy, 0, count);
		Peer.incoming.Add(copy);
	}

	public override void Flush() {
	}

	public override long Seek(long offset, SeekOrigin origin) {
		throw new NotSupportedException();
	}

	public override void SetLength(long length) {
		throw new NotSupportedException();
	}

	protected override void Dispose(bool disposing) {
		incoming.CompleteAdding();
		base.Dispose(disposing);
	}
}
