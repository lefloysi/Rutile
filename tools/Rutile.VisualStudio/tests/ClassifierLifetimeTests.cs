using System;
using System.Collections;
using System.Linq;
using System.Reflection;
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

		var decoder = assembly.GetType("Rutile.VisualStudio.RutileSemanticDecoder", true);
		var decoded = (IEnumerable)decoder.GetMethod("Decode", BindingFlags.NonPublic | BindingFlags.Static).Invoke(null, new object[] { source, new JArray(0, 3, 1, 0, 0), new[] { "function" } });
		var token = decoded.Cast<object>().Single();
		var classification = (string)token.GetType().GetField("Classification", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(token);
		var coloredType = assembly.GetType("Rutile.VisualStudio.ColoredSnapshotSpan", true);
		var span = Activator.CreateInstance(coloredType, BindingFlags.NonPublic | BindingFlags.Instance, null, new object[] { new SnapshotSpan(snapshot, 3, 1), classification }, null);
		var spans = Array.CreateInstance(coloredType, 1);
		spans.SetValue(span, 0);
		retained.GetType().GetField("spans", BindingFlags.NonPublic | BindingFlags.Instance).SetValue(retained, spans);
		var actual = ((IClassifier)classifier).GetClassificationSpans(new SnapshotSpan(snapshot, 0, source.Length));
		if (!actual.Any(item => item.Span.Start.Position == 3 && item.ClassificationType.Classification == "rutile.function")) {
			throw new Exception("Late-attached compiler function binding was not returned as a function classification");
		}

		var format = Activator.CreateInstance(assembly.GetType("Rutile.VisualStudio.RutileFunctionFormat", true), true);
		var color = format.GetType().GetProperty("ForegroundColor").GetValue(format);
		if (color.ToString() != "#FFDCDCAA") {
			throw new Exception("Function format is not yellow: " + color);
		}

		Console.WriteLine("Classifier created before document: semantic colors attach after document creation PASS");
		return 0;
	}
}
