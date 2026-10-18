using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.IO;
using System.Linq;
using System.Windows.Media;
using System.Windows.Threading;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Classification;
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
					File.WriteAllLines(path!, observed);
					timer.Stop();
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
}
