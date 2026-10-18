using System;
using System.ComponentModel.Composition;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Threading;
using Microsoft.VisualStudio.PlatformUI;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.Threading;
using Microsoft.VisualStudio.Utilities;
using Newtonsoft.Json.Linq;

namespace Rutile.VisualStudio {
	[Export(typeof(IWpfTextViewMarginProvider)), Name("Rutile Template Context"), MarginContainer(PredefinedMarginNames.Top), ContentType(RutileContentType.Name), TextViewRole(PredefinedTextViewRoles.Document)]
	internal sealed class RutileTemplateContextMarginProvider : IWpfTextViewMarginProvider {
		[Import]
		internal ITextDocumentFactoryService Documents = null!;

		public IWpfTextViewMargin CreateMargin(IWpfTextViewHost host, IWpfTextViewMargin container) {
			return new RutileTemplateContextMargin(host.TextView, Documents);
		}
	}

	internal sealed class RutileTemplateContextMargin : IWpfTextViewMargin {
		private readonly IWpfTextView view;
		private readonly ITextDocumentFactoryService documents;
		private readonly StackPanel panel;
		private readonly ComboBox choices;
		private readonly DispatcherTimer timer;
		private CancellationTokenSource? pending;
		private bool updating;
		private bool disposed;

		internal RutileTemplateContextMargin(IWpfTextView view, ITextDocumentFactoryService documents) {
			this.view = view;
			this.documents = documents;
			panel = new StackPanel { Orientation = Orientation.Horizontal, Visibility = Visibility.Collapsed, Margin = new Thickness(4) };
			panel.SetResourceReference(Panel.BackgroundProperty, EnvironmentColors.ToolWindowBackgroundBrushKey);
			var label = new TextBlock { Text = "Template context:", VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(0, 0, 8, 0) };
			label.SetResourceReference(TextBlock.ForegroundProperty, EnvironmentColors.ToolWindowTextBrushKey);
			panel.Children.Add(label);
			choices = new ComboBox { MinWidth = 320, MaxWidth = 1000, DisplayMemberPath = "Label", ToolTip = "Choose the instantiated types used for hover, completion, and navigation inside this template." };
			panel.Children.Add(choices);
			timer = new DispatcherTimer(TimeSpan.FromMilliseconds(175), DispatcherPriority.Background, OnTimer, view.VisualElement.Dispatcher);
			timer.Stop();
			choices.SelectionChanged += OnSelection;
			view.Caret.PositionChanged += OnCaret;
			view.TextBuffer.Changed += OnText;
			RutileEditorConnection.Instance.Changed += OnIndex;
			Schedule();
		}

		public FrameworkElement VisualElement => panel;
		public double MarginSize => panel.ActualHeight;
		public bool Enabled => !disposed;

		public ITextViewMargin? GetTextViewMargin(string name) {
			return name == "Rutile Template Context" ? this : null;
		}

		private void Schedule() {
			if (disposed) {
				return;
			}
			timer.Stop();
			timer.Start();
		}

		private void OnCaret(object sender, CaretPositionChangedEventArgs args) {
			Schedule();
		}

		private void OnText(object sender, TextContentChangedEventArgs args) {
			Schedule();
		}

		private void OnIndex(object? sender, EventArgs args) {
			ThreadHelper.JoinableTaskFactory.RunAsync(async () => {
				await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
				Schedule();
			}).FileAndForget("Rutile/TemplateContextRefresh");
		}

		private JObject? Parameters() {
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
			ThreadHelper.JoinableTaskFactory.RunAsync(RefreshAsync).FileAndForget("Rutile/TemplateContextRefresh");
		}

		private async Task RefreshAsync() {
			pending?.Cancel();
			pending?.Dispose();
			var cancellation = pending = new CancellationTokenSource();
			var rpc = RutileEditorConnection.Instance.Rpc;
			var parameters = Parameters();
			if (rpc == null || parameters == null) {
				panel.Visibility = Visibility.Collapsed;
				return;
			}
			try {
				var response = await rpc.InvokeWithParameterObjectAsync<JToken?>("rutile/templateContexts", parameters, cancellation.Token);
				if (disposed || cancellation.IsCancellationRequested) {
					return;
				}
				if (!(response is JObject result)) {
					return;
				}
				updating = true;
				choices.Items.Clear();
				choices.Items.Add(new ContextChoice("", "Template declaration (generic)"));
				var selected = (string?)result?["selected"] ?? "";
				choices.SelectedIndex = 0;
				if (result?["items"] is JArray items) {
					foreach (var item in items) {
						var choice = new ContextChoice((string?)item["key"] ?? "", (string?)item["label"] ?? "");
						choices.Items.Add(choice);
						if (choice.Key == selected) {
							choices.SelectedItem = choice;
						}
					}
				}
				panel.Visibility = choices.Items.Count > 1 ? Visibility.Visible : Visibility.Collapsed;
			}
			catch (Exception error) when (error is OperationCanceledException || error is StreamJsonRpc.ConnectionLostException || error is StreamJsonRpc.RemoteInvocationException || error is ObjectDisposedException) {
				// A new caret location, project, or analysis can replace this request.
			}
			finally {
				updating = false;
			}
		}

		private void OnSelection(object sender, SelectionChangedEventArgs args) {
			if (updating || disposed || !(choices.SelectedItem is ContextChoice choice)) {
				return;
			}
			ThreadHelper.JoinableTaskFactory.RunAsync(() => SelectAsync(choice)).FileAndForget("Rutile/TemplateContextSelection");
		}

		private async Task SelectAsync(ContextChoice choice) {
			var rpc = RutileEditorConnection.Instance.Rpc;
			var parameters = Parameters();
			if (rpc == null || parameters == null) {
				return;
			}
			parameters["context"] = choice.Key;
			try {
				await rpc.InvokeWithParameterObjectAsync<JToken?>("rutile/selectTemplateContext", parameters);
			}
			catch (Exception error) when (error is OperationCanceledException || error is StreamJsonRpc.ConnectionLostException || error is StreamJsonRpc.RemoteInvocationException || error is ObjectDisposedException) {
				Schedule();
			}
		}

		public void Dispose() {
			disposed = true;
			timer.Stop();
			pending?.Cancel();
			pending?.Dispose();
			choices.SelectionChanged -= OnSelection;
			view.Caret.PositionChanged -= OnCaret;
			view.TextBuffer.Changed -= OnText;
			RutileEditorConnection.Instance.Changed -= OnIndex;
		}

		private sealed class ContextChoice {
			internal string Key { get; }
			public string Label { get; }

			internal ContextChoice(string key, string label) {
				Key = key;
				Label = label;
			}
		}
	}
}
