using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using Microsoft.VisualStudio.Editor;
using Microsoft.VisualStudio.Language.Intellisense.AsyncCompletion;
using Microsoft.VisualStudio.OLE.Interop;
using System.Windows.Media;
using System.Windows.Threading;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Classification;
using Microsoft.VisualStudio.Text.BraceCompletion;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.Utilities;

[Export(typeof(Microsoft.VisualStudio.LanguageServer.Client.ILanguageClient)), ContentType("RutilePaletteProbe"), Name("Rutile palette verification")]
public sealed class PaletteProbe : Microsoft.VisualStudio.LanguageServer.Client.ILanguageClient {
	[Export, Name("RutilePaletteProbe"), BaseDefinition(Microsoft.VisualStudio.LanguageServer.Client.CodeRemoteContentDefinition.CodeRemoteBaseTypeName)]
	internal static ContentTypeDefinition ContentType = null!;
	[Export, FileExtension(".rprobe"), ContentType("RutilePaletteProbe")]
	internal static FileExtensionToContentTypeDefinition Extension = null!;
	[Import] internal IClassifierAggregatorService Classifiers = null!;
	[Import] internal IClassificationFormatMapService Formats = null!;
	[Import] internal IClassificationTypeRegistryService Types = null!;
	[Import] internal ITextDocumentFactoryService Documents = null!;
	[Import] internal IVsEditorAdaptersFactoryService Adapters = null!;
	[Import] internal IAsyncCompletionBroker Completion = null!;
	[Import] internal IBraceCompletionManagerFactory BraceManagers = null!;
	[ImportMany] internal IEnumerable<Lazy<IBraceCompletionDefaultProvider, IDictionary<string, object>>> DefaultBraces = null!;
	[ImportMany] internal IEnumerable<Lazy<IBraceCompletionContextProvider, IDictionary<string, object>>> ContextBraces = null!;
	public string Name => "Rutile palette verification";
	public IEnumerable<string> ConfigurationSections => Array.Empty<string>();
	public object InitializationOptions => null!;
	public IEnumerable<string> FilesToWatch => Array.Empty<string>();
	public bool ShowNotificationOnInitializeFailed => false;
	public event Microsoft.VisualStudio.Threading.AsyncEventHandler<EventArgs>? StartAsync;
	public event Microsoft.VisualStudio.Threading.AsyncEventHandler<EventArgs>? StopAsync;
	public System.Threading.Tasks.Task<Microsoft.VisualStudio.LanguageServer.Client.Connection?> ActivateAsync(System.Threading.CancellationToken token) {
		return System.Threading.Tasks.Task.FromResult<Microsoft.VisualStudio.LanguageServer.Client.Connection?>(null);
	}
	public System.Threading.Tasks.Task OnServerInitializedAsync() {
		return System.Threading.Tasks.Task.CompletedTask;
	}
	public System.Threading.Tasks.Task<Microsoft.VisualStudio.LanguageServer.Client.InitializationFailureContext?> OnServerInitializeFailedAsync(Microsoft.VisualStudio.LanguageServer.Client.ILanguageClientInitializationInfo information) {
		return System.Threading.Tasks.Task.FromResult<Microsoft.VisualStudio.LanguageServer.Client.InitializationFailureContext?>(null);
	}
	private IClassifierAggregatorService classifiers = null!;
	private IClassificationFormatMap map = null!;
	private string? path;
	private readonly HashSet<ITextBuffer> checkedBuffers = new HashSet<ITextBuffer>();

	private void Diagnostics(ITextBuffer buffer) {
		var lines = new List<string> { "utc=" + DateTime.UtcNow.ToString("O"), "content-type=" + buffer.ContentType.TypeName };
		try {
			lines.Add("source=" + (Documents.TryGetTextDocument(buffer, out var document) ? document.FilePath : "<no document>"));
			var assembly = AppDomain.CurrentDomain.GetAssemblies().FirstOrDefault(item => item.GetName().Name == "Rutile.VisualStudio");
			lines.Add("assembly=" + (assembly?.Location ?? "<not loaded>"));
			if (assembly != null) {
				var flags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static;
				var editorType = assembly.GetType("Rutile.VisualStudio.RutileEditorConnection", true);
				var editor = editorType.GetProperty("Instance", flags).GetValue(null);
				lines.Add("rpc-null=" + (editorType.GetProperty("Rpc", flags).GetValue(editor) == null));
				var storeType = assembly.GetType("Rutile.VisualStudio.RutileSemanticStore", true);
				var store = storeType.GetProperty("Instance", flags).GetValue(null);
				foreach (var name in new[] { "legend", "documents", "buffers" }) {
					var value = storeType.GetField(name, flags).GetValue(store);
					var collection = value as System.Collections.ICollection;
					lines.Add(name + "-count=" + (collection == null ? "<unknown>" : collection.Count.ToString()));
					if (name == "buffers" && value is System.Collections.IEnumerable buffers) {
						var index = 0;
						foreach (var reference in buffers) {
							var arguments = new object?[] { null };
							var alive = (bool)reference.GetType().GetMethod("TryGetTarget").Invoke(reference, arguments);
							var state = arguments[0];
							if (!alive || state == null) {
								lines.Add("buffer[" + index++ + "]=<collected>");
								continue;
							}
							var stateType = state.GetType();
							var spans = stateType.GetField("spans", flags).GetValue(state);
							var spanCollection = spans as System.Collections.ICollection;
							lines.Add("buffer[" + index++ + "]=" + stateType.GetProperty("Uri", flags).GetValue(state) + ";spans-count=" + (spanCollection == null ? "<unknown>" : spanCollection.Count.ToString()));
						}
					}
				}
			}
		}
		catch (Exception error) {
			lines.Add(error.ToString());
		}
		File.WriteAllLines(path + ".diagnostics", lines);
	}

	public async System.Threading.Tasks.Task OnLoadedAsync() {
		await Microsoft.VisualStudio.Shell.ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
		path = Environment.GetEnvironmentVariable("RUTILE_PALETTE_RESULT");
		if (string.IsNullOrEmpty(path)) {
			return;
		}
		classifiers = Classifiers;
		map = Formats.GetClassificationFormatMap("text");
		var types = Types;
		var brushes = new List<string>();
		foreach (var name in new[] { "rutile.namespace", "rutile.type", "rutile.function", "rutile.keyword", "rutile.control" }) {
			var type = types.GetClassificationType(name);
			var brush = type == null ? null : map.GetTextProperties(type).ForegroundBrush as SolidColorBrush;
			brushes.Add(name + "=" + brush?.Color.ToString());
		}
		File.WriteAllLines(path + ".brushes", brushes);
		Documents.TextDocumentCreated += (_, args) => {
			if (args.TextDocument.FilePath.EndsWith("main.rt", StringComparison.OrdinalIgnoreCase)) {
				Begin(args.TextDocument.TextBuffer);
			}
		};
		File.WriteAllText(path + ".ready", "ready");
	}

	private void Begin(ITextBuffer buffer) {
		if (!checkedBuffers.Add(buffer)) {
			return;
		}
		var classifier = classifiers.GetClassifier(buffer);
		var timer = new DispatcherTimer(DispatcherPriority.Normal) { Interval = TimeSpan.FromMilliseconds(250) };
		var tries = 0;
		var expected = new Dictionary<string, string> {
			["sample {"] = "rutile.namespace",
			["Item {"] = "rutile.type",
			["value()"] = "rutile.function",
			["read(Item"] = "rutile.function",
			["executable;"] = "rutile.function",
			["i32 field"] = "rutile.keyword",
			["return this"] = "rutile.control"
		};
		var last = "waiting";
		timer.Tick += (_, __) => {
			try {
				if (tries % 12 == 0) {
					Diagnostics(buffer);
				}
				var snapshot = buffer.CurrentSnapshot;
				var source = snapshot.GetText();
				var spans = classifier.GetClassificationSpans(new SnapshotSpan(snapshot, 0, snapshot.Length));
				var observed = new List<string>();
				foreach (var item in expected) {
					var offset = source.IndexOf(item.Key, StringComparison.Ordinal);
					var styled = spans.FirstOrDefault(span => span.Span.Start.Position <= offset && span.Span.End.Position > offset && span.ClassificationType.IsOfType(item.Value));
					if (styled == null) {
						last = "missing " + item.Key + ": " + string.Join(",", spans.Where(span => span.Span.Start.Position <= offset && span.Span.End.Position > offset).Select(span => span.ClassificationType.Classification));
						break;
					}
					var brush = map.GetTextProperties(styled.ClassificationType).ForegroundBrush as SolidColorBrush;
					observed.Add(item.Key + "=" + item.Value + ":" + brush?.Color.ToString() + ";actual=" + styled.ClassificationType.Classification);
				}
				if (observed.Count == expected.Count) {
					var timing = System.Diagnostics.Stopwatch.StartNew();
					for (var query = 0; query < 100; ++query) {
						classifier.GetClassificationSpans(new SnapshotSpan(snapshot, 0, snapshot.Length));
					}
					timing.Stop();
					observed.Add("classification-call-average-ms=" + (timing.Elapsed.TotalMilliseconds / 100).ToString("F4", System.Globalization.CultureInfo.InvariantCulture));
					var offset = source.IndexOf("value()", StringComparison.Ordinal);
					using (var edit = buffer.CreateEdit()) {
						edit.Insert(0, "\n");
						edit.Apply();
					}
					var after = buffer.CurrentSnapshot;
					var translated = classifier.GetClassificationSpans(new SnapshotSpan(after, 0, after.Length));
					var retained = translated.Any(span => span.Span.Start.Position == offset + 1 && span.ClassificationType.IsOfType("rutile.function"));
					observed.Add("cached-span-translation=" + retained);
					timer.Stop();
					if (Environment.GetEnvironmentVariable("RUTILE_TYPING_TEST") == "1") {
						BeginTyping(buffer, observed);
					}
					else {
						File.WriteAllLines(path!, observed);
					}
					return;
				}
				if (++tries >= 240) {
					File.WriteAllText(path!, "FAILED: " + last);
					timer.Stop();
				}
			}
			catch (Exception error) {
				File.WriteAllText(path!, "FAILED: " + error);
				timer.Stop();
			}
		};
		timer.Start();
	}

	private void Type(IWpfTextView view, char? character) {
		var target = (IOleCommandTarget)Adapters.GetViewAdapter(view);
		var group = Microsoft.VisualStudio.VSConstants.VSStd2K;
		var value = IntPtr.Zero;
		try {
			if (character.HasValue && character.Value != '\n' && character.Value != '\t') {
				value = Marshal.AllocCoTaskMem(32);
				Marshal.GetNativeVariantForObject((ushort)character.Value, value);
			}
			var command = !character.HasValue ? Microsoft.VisualStudio.VSConstants.VSStd2KCmdID.BACKSPACE
				: character.Value == '\n' ? Microsoft.VisualStudio.VSConstants.VSStd2KCmdID.RETURN
				: character.Value == '\t' ? Microsoft.VisualStudio.VSConstants.VSStd2KCmdID.TAB
				: Microsoft.VisualStudio.VSConstants.VSStd2KCmdID.TYPECHAR;
			Marshal.ThrowExceptionForHR(target.Exec(ref group, (uint)command, 0, value, IntPtr.Zero));
		}
		finally {
			if (value != IntPtr.Zero) { Marshal.FreeCoTaskMem(value); }
		}
	}



	private static int Columns(string text, int tabSize) {
		var column = 0;
		foreach (var value in text) { column = value == '\t' ? column + tabSize - column % tabSize : column + 1; }
		return column;
	}

	private string BraceDiagnostics(IWpfTextView view, int start) {
		var details = new List<string> { "start=" + start, "caret=" + view.Caret.Position.BufferPosition.Position,
			"content-type=" + view.TextBuffer.ContentType.TypeName,
			"actual-text=" + view.TextBuffer.CurrentSnapshot.GetText().Replace("\r", "\\r").Replace("\n", "\\n") };
		try {
			details.Add("brace-option=" + view.Options.GetOptionValue<bool>(DefaultTextViewOptions.BraceCompletionEnabledOptionName));
			var manager = BraceManagers.TryGetBraceCompletionManager(view);
			details.Add("brace-manager=" + (manager == null ? "<null>" : manager.GetType().FullName));
			if (manager != null) {
				details.Add("brace-manager-enabled=" + manager.Enabled + ";sessions=" + manager.ActiveSessionCount + ";opening=" + string.Join(",", manager.OpeningBraces));
			}
			foreach (var provider in DefaultBraces) {
				if (provider.Metadata.TryGetValue("ContentTypes", out var names) && names is IEnumerable<string> contentTypes && contentTypes.Contains("Rutile")) {
					details.Add("default-provider=" + provider.Value.GetType().FullName);
				}
			}
			foreach (var provider in ContextBraces) {
				if (provider.Metadata.TryGetValue("ContentTypes", out var names) && names is IEnumerable<string> contentTypes && contentTypes.Contains("Rutile")) {
					details.Add("context-provider=" + provider.Value.GetType().FullName);
					var point = new SnapshotPoint(view.TextSnapshot, Math.Min(start, view.TextSnapshot.Length));
					var accepted = provider.Value.TryCreateContext(view, point, '(', ')', out var context);
					details.Add("context-at-opening=" + accepted + ";context=" + context?.GetType().FullName);
				}
			}
		}
		catch (Exception error) { details.Add("brace-diagnostic-error=" + error); }
		return string.Join("; ", details);
	}

	private void BeginTyping(ITextBuffer buffer, List<string> observed) {
		var view = buffer.Properties.GetProperty<IWpfTextView>(typeof(PaletteTypingView));
		var snapshot = buffer.CurrentSnapshot;
		var text = snapshot.GetText();
		var marker = text.IndexOf("// typing cursor", StringComparison.Ordinal);
		var start = marker < 0 ? -1 : text.IndexOf('\n', marker) + 1;
		if (start <= 0) { throw new InvalidOperationException("Missing typing fixture"); }
		while (start < text.Length && (text[start] == ' ' || text[start] == '\t')) { ++start; }
		if (start >= text.Length || (text[start] != '\r' && text[start] != '\n')) { throw new InvalidOperationException("Typing fixture must be a blank EOL"); }
		view.Caret.MoveTo(new SnapshotPoint(snapshot, start));
		string[] items = Array.Empty<string>();
		var triggered = false;
		EventHandler<Microsoft.VisualStudio.Language.Intellisense.AsyncCompletion.Data.CompletionTriggeredEventArgs> handler = (_, args) => {
			if (args.TextView != view) { return; }
			triggered = true;
			args.CompletionSession.ItemsUpdated += (sender, update) => {
				System.Threading.Interlocked.Exchange(ref items, update.Items.Items.Select(item => item.DisplayText).ToArray());
			};
		};
		Completion.CompletionTriggered += handler;
		// Only a real TYPECHAR command starts completion; the probe never invokes completion itself.
		Type(view, 'c');
		var timer = new DispatcherTimer(DispatcherPriority.Normal) { Interval = TimeSpan.FromMilliseconds(250) };
		var tries = 0;
		timer.Tick += (_, __) => {
			try {
				var current = System.Threading.Volatile.Read(ref items);
				string? completionFailure = null;
				if (!current.Contains("completion_local")) {
					if (++tries < 80) { return; }
					completionFailure = "Normal identifier typing did not offer completion_local; triggered=" + triggered + "; supported=" + Completion.IsCompletionSupported(buffer.ContentType, view.Roles) + "; items=" + string.Join(",", current);
				}
				observed.Add("automatic-identifier-completion=" + (completionFailure == null));
				observed.Add("automatic-completion-items=" + string.Join(",", current));
				Completion.GetSession(view)?.Dismiss();
				Completion.CompletionTriggered -= handler;
				using (var edit = buffer.CreateEdit()) { edit.Delete(start, 1); edit.Apply(); }
				view.Caret.MoveTo(new SnapshotPoint(buffer.CurrentSnapshot, start));
				var before = buffer.CurrentSnapshot.GetText();
				Type(view, '(');
				if (buffer.CurrentSnapshot.GetText() != before.Insert(start, "()") || view.Caret.Position.BufferPosition.Position != start + 1) { throw new InvalidOperationException("Native parenthesis insertion failed; " + BraceDiagnostics(view, start)); }
				observed.Add("native-brace-pair=True");
				Type(view, null);
				if (buffer.CurrentSnapshot.GetText() != before) { throw new InvalidOperationException("Native paired backspace failed; " + BraceDiagnostics(view, start)); }
				observed.Add("native-brace-paired-backspace=True");
				Type(view, '(');
				Type(view, ')');
				if (buffer.CurrentSnapshot.GetText() != before.Insert(start, "()") || view.Caret.Position.BufferPosition.Position != start + 2) { throw new InvalidOperationException("Native closing-brace overtype failed; " + BraceDiagnostics(view, start)); }
				observed.Add("native-brace-overtype=True");

				using (var edit = buffer.CreateEdit()) { edit.Delete(start, 2); edit.Apply(); }
				view.Caret.MoveTo(new SnapshotPoint(buffer.CurrentSnapshot, start));
				Type(view, '(');
				var beforeTab = buffer.CurrentSnapshot.GetText();
				Type(view, '\t');
				if (buffer.CurrentSnapshot.GetText() != beforeTab || view.Caret.Position.BufferPosition.Position != start + 2) { throw new InvalidOperationException("Native Tab past same-line closing brace failed; " + BraceDiagnostics(view, start)); }
				observed.Add("native-brace-tab=True");
				using (var edit = buffer.CreateEdit()) { edit.Delete(start, 2); edit.Apply(); }
				view.Caret.MoveTo(new SnapshotPoint(buffer.CurrentSnapshot, start));
				var openingLine = buffer.CurrentSnapshot.GetLineFromPosition(start);
				var indent = openingLine.GetText();
				Type(view, '{');
				Type(view, '\n');
				var updated = buffer.CurrentSnapshot;
				openingLine = updated.GetLineFromPosition(start);
				var bodyLine = updated.GetLineFromLineNumber(openingLine.LineNumber + 1);
				var closingLine = updated.GetLineFromLineNumber(openingLine.LineNumber + 2);
				var tabSize = Math.Max(1, view.Options.GetOptionValue(DefaultOptions.TabSizeOptionId));
				var bodyColumn = Columns(indent, tabSize) + Math.Max(1, view.Options.GetOptionValue(DefaultOptions.IndentSizeOptionId));
				if (openingLine.GetText() != indent + "{" || closingLine.GetText() != indent + "}" || !string.IsNullOrWhiteSpace(bodyLine.GetText()) || Columns(bodyLine.GetText(), tabSize) != bodyColumn || view.Caret.Position.BufferPosition.Position != bodyLine.End.Position) {
					throw new InvalidOperationException("Native empty-brace Return indentation/caret failed; " + BraceDiagnostics(view, start));
				}
				observed.Add("native-brace-return=True");
				if (completionFailure != null) { observed.Add("FAILED: " + completionFailure); }

				File.WriteAllLines(path!, observed);
				timer.Stop();
			}
			catch (Exception error) {
				Completion.CompletionTriggered -= handler;
				File.WriteAllText(path!, string.Join(Environment.NewLine, observed) + Environment.NewLine + "FAILED: " + error);
				timer.Stop();
			}
		};
		timer.Start();
	}

}


[Export(typeof(IWpfTextViewCreationListener)), ContentType("Rutile"), TextViewRole(PredefinedTextViewRoles.Editable)]
public sealed class PaletteTypingView : IWpfTextViewCreationListener {
	public void TextViewCreated(IWpfTextView textView) {
		textView.TextBuffer.Properties[typeof(PaletteTypingView)] = textView;
	}
}
