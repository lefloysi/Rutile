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
		private readonly Canvas adornment;
		private readonly StackPanel row;
		private readonly TextBlock title;
		private readonly TextBox arguments;
		private readonly TextBlock status;
		private CancellationTokenSource? pending;
		private ITrackingPoint? anchor;
		private JObject? templatePosition;
		private bool disposed;

		internal RutileTemplateContextTagger(IWpfTextView view, ITextDocumentFactoryService documents) {
			this.view = view;
			this.documents = documents;
			row = TemplateArgumentControls.CreateRow();
			row.SetResourceReference(Panel.BackgroundProperty, EnvironmentColors.ToolWindowBackgroundBrushKey);
			title = new TextBlock { VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(0, 0, 4, 0) };
			title.SetResourceReference(TextBlock.ForegroundProperty, EnvironmentColors.ToolWindowTextBrushKey);
			row.Children.Add(title);
			arguments = TemplateArgumentControls.CreateInput();
			arguments.KeyDown += OnArgumentKey;
			row.Children.Add(arguments);
			var close = new TextBlock { Text = ">", VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(4, 0, 0, 0) };
			close.SetResourceReference(TextBlock.ForegroundProperty, EnvironmentColors.ToolWindowTextBrushKey);
			row.Children.Add(close);
			var apply = new Button { Content = "Apply", Margin = new Thickness(4, 0, 0, 0), Padding = new Thickness(6, 0, 6, 0) };
			apply.Click += (_, __) => Submit(false);
			row.Children.Add(apply);
			var reset = new Button { Content = "Generic", Margin = new Thickness(4, 0, 0, 0), Padding = new Thickness(6, 0, 6, 0) };
			reset.Click += (_, __) => Submit(true);
			row.Children.Add(reset);
			status = new TextBlock { VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(8, 0, 0, 0), MaxWidth = 400, TextTrimming = TextTrimming.CharacterEllipsis };
			status.SetResourceReference(TextBlock.ForegroundProperty, EnvironmentColors.ToolWindowTextBrushKey);
			row.Children.Add(status);
			// Negotiate a separate row above the declaration; consume no source width.
			adornment = new Canvas { Width = 0, Height = 0, ClipToBounds = false };
			Canvas.SetTop(row, 0);
			adornment.Children.Add(row);
			timer = new DispatcherTimer(TimeSpan.FromMilliseconds(175), DispatcherPriority.Background, OnTimer, view.VisualElement.Dispatcher);
			timer.Stop();
			view.Caret.PositionChanged += OnCaret;
			view.TextBuffer.Changed += OnText;
			view.Closed += OnClosed;
			view.LayoutChanged += OnLayout;
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
				row.Measure(new Size(double.PositiveInfinity, double.PositiveInfinity));
				var geometry = new TemplateAdornmentGeometry(row.DesiredSize.Height, TextBaseline(point));
				Canvas.SetTop(row, geometry.CanvasTop);
				yield return new TagSpan<IntraTextAdornmentTag>(span, new IntraTextAdornmentTag(adornment, null, geometry.TopSpace, 0, 0, 0, PositionAffinity.Predecessor));
			}
		}

		private double TextBaseline(SnapshotPoint point) {
			if (view.TextViewLines.IsValid && view.TextViewLines.ContainsBufferPosition(point)) {
				var line = view.TextViewLines.GetTextViewLineContainingBufferPosition(point);
				return Math.Max(0, line.Baseline);
			}
			return 0;
		}

		private void OnLayout(object sender, TextViewLayoutChangedEventArgs args) {
			if (disposed || anchor == null || !view.TextViewLines.IsValid) {
				return;
			}
			var point = anchor.GetPoint(view.TextSnapshot);
			if (!view.TextViewLines.ContainsBufferPosition(point)) {
				return;
			}
			var line = view.TextViewLines.GetTextViewLineContainingBufferPosition(point);
			try {
				// Measure the actual placed adornment, accounting for editor font and DPI.
				var placed = row.TransformToAncestor(view.VisualElement).Transform(new Point(0, 0));
				var desired = line.TextTop - view.ViewportTop - row.ActualHeight - 4;
				var difference = desired - placed.Y;
				if (Math.Abs(difference) > 0.25) {
					Canvas.SetTop(row, Canvas.GetTop(row) + difference);
				}
			}
			catch (InvalidOperationException) {
				// The editor may not have attached this adornment during the current layout.
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
				anchor = snapshot.CreateTrackingPoint(line.Start, PointTrackingMode.Positive);
				templatePosition = parameters;
				title.Text = ((string?)declaration["name"] ?? "Template") + " <";
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

		private void OnArgumentKey(object sender, KeyEventArgs args) {
			if (args.Key == Key.Enter) {
				args.Handled = true;
				Submit(false);
			}
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
			view.LayoutChanged -= OnLayout;
			RutileEditorConnection.Instance.Changed -= OnIndex;
			arguments.KeyDown -= OnArgumentKey;
		}
	}
}
