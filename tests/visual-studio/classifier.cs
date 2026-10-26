using System;
using System.Reflection;
using System.Runtime.Remoting.Messaging;
using System.Runtime.Remoting.Proxies;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Classification;
using Microsoft.VisualStudio.Utilities;
using Newtonsoft.Json;
using Rutile.VisualStudio;

internal sealed class EditorInterface<T> : RealProxy {
	private readonly Func<MethodInfo, object[], object?> invoke;
	internal EditorInterface(Func<MethodInfo, object[], object?> invoke) : base(typeof(T)) {
		this.invoke = invoke;
	}
	internal T Value => (T)GetTransparentProxy();
	public override IMessage Invoke(IMessage message) {
		var call = (IMethodCallMessage)message;
		var arguments = call.Args;
		try {
			return new ReturnMessage(invoke((MethodInfo)call.MethodBase, arguments), arguments, arguments.Length, call.LogicalCallContext, call);
		}
		catch (Exception error) {
			return new ReturnMessage(error, call);
		}
	}
}

public static partial class EditorObservations {
	public static string ClassifierLifetime() {
		var properties = new PropertyCollection();
		ITextSnapshot? snapshot = null;
		ITextBuffer? buffer = null;
		buffer = new EditorInterface<ITextBuffer>((method, values) => method.Name == "get_Properties" ? (object)properties : method.Name == "get_CurrentSnapshot" ? snapshot : null).Value;
		snapshot = new EditorInterface<ITextSnapshot>((method, values) => {
			if (method.Name == "get_Length") {
				return 7;
			}
			if (method.Name == "get_TextBuffer") {
				return buffer;
			}
			if (method.Name == "GetText") {
				if (values.Length == 0) {
					return "fn f();";
				}
				if (values.Length == 1) {
					var span = (Span)values[0];
					return "fn f();".Substring(span.Start, span.Length);
				}
				return "fn f();".Substring((int)values[0], (int)values[1]);
			}
			return null;
		}).Value;
		var path = System.IO.Path.Combine(System.IO.Path.GetTempPath(), "rutile-editor-late.rt");
		var document = new EditorInterface<ITextDocument>((method, values) => method.Name == "get_TextBuffer" ? (object)buffer : method.Name == "get_FilePath" ? path : null).Value;
		EventHandler<TextDocumentEventArgs>? created = null;
		var available = false;
		var subscriptions = 0;
		var factory = new EditorInterface<ITextDocumentFactoryService>((method, values) => {
			if (method.Name == "TryGetTextDocument") {
				values[1] = available ? document : null!;
				return available;
			}
			if (method.Name == "add_TextDocumentCreated") {
				created += (EventHandler<TextDocumentEventArgs>)values[0];
				++subscriptions;
			}
			if (method.Name == "remove_TextDocumentCreated") {
				created -= (EventHandler<TextDocumentEventArgs>)values[0];
			}
			return null;
		}).Value;
		var registry = new EditorInterface<IClassificationTypeRegistryService>((method, values) => method.Name == "GetClassificationType"
			? new EditorInterface<IClassificationType>((member, arguments) => member.Name == "get_Classification" ? values[0] : null).Value : null).Value;
		var provider = new RutileLexicalClassifierProvider { Documents = factory, Types = registry };
		var classifier = provider.GetClassifier(buffer);
		var field = classifier.GetType().GetField("semantic", BindingFlags.NonPublic | BindingFlags.Instance)!;
		var before = field.GetValue(classifier) != null;
		var sameClassifier = ReferenceEquals(classifier, provider.GetClassifier(buffer));
		var other = new EditorInterface<ITextBuffer>((method, values) => null).Value;
		var otherDocument = new EditorInterface<ITextDocument>((method, values) => method.Name == "get_TextBuffer" ? (object)other : method.Name == "get_FilePath" ? path : null).Value;
		created?.Invoke(null, new TextDocumentEventArgs(otherDocument));
		var unrelated = field.GetValue(classifier) != null;
		available = true;
		created?.Invoke(null, new TextDocumentEventArgs(document));
		var attached = field.GetValue(classifier);
		created?.Invoke(null, new TextDocumentEventArgs(document));
		var failedProperties = new PropertyCollection();
		var failedBuffer = new EditorInterface<ITextBuffer>((method, values) => method.Name == "get_Properties" ? (object)failedProperties : method.Name == "get_CurrentSnapshot" ? snapshot : null).Value;
		var failedDocument = new EditorInterface<ITextDocument>((method, values) => {
			if (method.Name == "get_TextBuffer") {
				return failedBuffer;
			}
			if (method.Name == "get_FilePath") {
				throw new InvalidOperationException("semantic attachment failure");
			}
			return null;
		}).Value;
		var failedFactory = new EditorInterface<ITextDocumentFactoryService>((method, values) => {
			if (method.Name == "TryGetTextDocument") {
				values[1] = failedDocument;
				return true;
			}
			return null;
		}).Value;
		string? attachmentError = null;
		var failedProvider = new RutileLexicalClassifierProvider { Documents = failedFactory, Types = registry, ReportError = message => attachmentError = message };
		var failedClassifier = failedProvider.GetClassifier(failedBuffer);
		var lexicalAfterFailure = failedClassifier.GetClassificationSpans(new SnapshotSpan(snapshot, 0, snapshot.Length)).Count;
		return JsonConvert.SerializeObject(new {
			lexical_after_attachment_failure = lexicalAfterFailure,
			attachment_error = attachmentError,
			attachment_error_reported = attachmentError?.Contains("semantic attachment failure") == true,
			before, unrelated, attached = attached != null, same_classifier = sameClassifier, subscriptions,
			retained = ReferenceEquals(attached, field.GetValue(classifier)),
			uri = attached?.GetType().GetProperty("Uri", BindingFlags.NonPublic | BindingFlags.Instance)?.GetValue(attached),
			expected_uri = new Uri(path).AbsoluteUri
		});
	}
}
