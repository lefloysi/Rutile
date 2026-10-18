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
	static int Main(string[] args) {
		SynchronizationContext.SetSynchronizationContext(new SynchronizationContext());
		var properties = new PropertyCollection();
		const string source = "fn f();";
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
				var languageClient = (ILanguageClient)Activator.CreateInstance(assembly.GetType("Rutile.VisualStudio.RutileLanguageClient", true), true);
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
			}

		}

		var actual = ((IClassifier)classifier).GetClassificationSpans(new SnapshotSpan(snapshot, 0, source.Length));
		if (!actual.Any(item => item.Span.Start.Position == 3 && item.ClassificationType.Classification == "rutile.function")) {
			throw new Exception("Late-attached compiler function binding was not returned as a function classification");
		}

		var format = Activator.CreateInstance(assembly.GetType("Rutile.VisualStudio.RutileFunctionFormat", true), true);
		var color = format.GetType().GetProperty("ForegroundColor").GetValue(format);
		if (color.ToString() != "#FFDCDCAA") {
			throw new Exception("Function format is not yellow: " + color);
		}

		Console.WriteLine("Public Attach/OnServerInitialized + didOpen + real JSON-RPC tokens + late document function classification PASS");
		return 0;
	}
}

sealed class SemanticServer {
	internal bool Opened;
	internal int Legends;
	internal int Requests;

	[JsonRpcMethod("textDocument/didOpen", UseSingleObjectParameterDeserialization = true)]
	public void Open(JToken parameters) {
		Opened = (string)parameters["textDocument"]["text"] == "fn f();";
	}

	[JsonRpcMethod("rutile/semanticTokensLegend", UseSingleObjectParameterDeserialization = true)]
	public JObject Legend(JToken parameters) {
		Interlocked.Increment(ref Legends);
		return new JObject { ["tokenTypes"] = new JArray("function"), ["tokenModifiers"] = new JArray() };
	}

	[JsonRpcMethod("textDocument/semanticTokens/full", UseSingleObjectParameterDeserialization = true)]
	public JObject Tokens(JToken parameters) {
		Interlocked.Increment(ref Requests);
		return new JObject { ["data"] = new JArray(0, 3, 1, 0, 0) };
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
