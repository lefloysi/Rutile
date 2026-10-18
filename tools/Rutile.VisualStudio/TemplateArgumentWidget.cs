using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Media;
using System.Windows.Documents;
using Newtonsoft.Json.Linq;

namespace Rutile.VisualStudio {
	internal sealed class TemplateArgumentWidget : ContentControl {
		internal event Action? Apply;
		internal event Action? Clear;
		internal event Action<string>? Select;
		internal TextBox Arguments { get; } = new TextBox();
		internal TextBlock Status { get; } = new TextBlock();
		private readonly TextBlock label = new TextBlock { VerticalAlignment = VerticalAlignment.Center };
		private readonly Button dropdown = new Button { Content = "Provide sample template arguments for IntelliSense  ▾", Padding = new Thickness(6, 1, 6, 1) };
		private readonly Popup popup = new Popup { StaysOpen = false, Placement = PlacementMode.Bottom, AllowsTransparency = true };
		private readonly StackPanel fields = new StackPanel();
		private readonly List<TextBox> inputs = new List<TextBox>();
		private readonly ContextMenu menu = new ContextMenu();
		private string parameterKey = "";
		private string acceptedArguments = "";
		private readonly Dictionary<string, string> samples = new Dictionary<string, string>();
		private JArray existing = new JArray();
		private readonly Dictionary<string, string> choices = new Dictionary<string, string>();

		internal TemplateArgumentWidget() {
			Margin = new Thickness(12, 0, 0, 0);
			var row = new StackPanel { Orientation = Orientation.Horizontal };
			row.Children.Add(label);
			dropdown.Margin = new Thickness(6, 0, 0, 0);
			dropdown.Click += (_, __) => {
				menu.PlacementTarget = dropdown;
				menu.IsOpen = true;
			};
			row.Children.Add(dropdown);
			var pencil = new Button { Content = "✎", ToolTip = "Edit sample template arguments", Padding = new Thickness(5, 0, 5, 0), Margin = new Thickness(3, 0, 0, 0) };
			pencil.Click += (_, __) => {
				popup.IsOpen = true;
			};
			row.Children.Add(pencil);
			Content = row;
			foreach (var child in row.Children) {
				if (child is Button button) {
					ThemeButton(button);
				}
			}
			popup.PlacementTarget = pencil;
			var panel = new StackPanel { Margin = new Thickness(10) };
			panel.Children.Add(new TextBlock { Text = "Sample template arguments", Margin = new Thickness(0, 0, 0, 8) });
			panel.Children.Add(fields);
			Status.MaxWidth = 380;
			Status.TextWrapping = TextWrapping.Wrap;
			panel.Children.Add(Status);
			var apply = new Button { Content = "Apply", HorizontalAlignment = HorizontalAlignment.Right, Padding = new Thickness(12, 2, 12, 2), Margin = new Thickness(0, 8, 0, 0) };
			ThemeButton(apply);
			apply.Click += (_, __) => {
				Arguments.Text = JoinArguments(inputs.ConvertAll(input => input.Text));
				Apply?.Invoke();
				popup.IsOpen = false;
			};
			panel.Children.Add(apply);
			popup.Child = new Border { Background = Background, BorderBrush = Brushes.Gray, BorderThickness = new Thickness(1), Child = panel };
			Loaded += (_, __) => {
				((Border)popup.Child).Background = Background;
				((Border)popup.Child).SetValue(TextElement.ForegroundProperty, Foreground);
				menu.Background = Background;
				menu.Foreground = Foreground;
				menu.OverridesDefaultStyle = true;
				var menuBorder = new FrameworkElementFactory(typeof(Border));
				menuBorder.SetValue(Border.BackgroundProperty, new TemplateBindingExtension(BackgroundProperty));
				menuBorder.SetValue(Border.PaddingProperty, new Thickness(4));
				menuBorder.AppendChild(new FrameworkElementFactory(typeof(ItemsPresenter)));
				menu.Template = new ControlTemplate(typeof(ContextMenu)) { VisualTree = menuBorder };
			};
		}

		private void ThemeButton(Button button) {
			button.SetBinding(BackgroundProperty, new System.Windows.Data.Binding("Background") { Source = this });
			button.SetBinding(ForegroundProperty, new System.Windows.Data.Binding("Foreground") { Source = this });
			var border = new FrameworkElementFactory(typeof(Border));
			border.SetValue(Border.BackgroundProperty, new TemplateBindingExtension(BackgroundProperty));
			border.SetValue(Border.PaddingProperty, new TemplateBindingExtension(PaddingProperty));
			border.SetValue(Border.BorderBrushProperty, Brushes.DimGray);
			border.SetValue(Border.BorderThicknessProperty, new Thickness(1));
			var content = new FrameworkElementFactory(typeof(ContentPresenter));
			border.AppendChild(content);
			button.Template = new ControlTemplate(typeof(Button)) { VisualTree = border };
		}

		internal void Update(JObject declaration, JObject result, string identity) {
			var parameters = declaration["parameters"]?.ToObject<string[]>() ?? Array.Empty<string>();
			label.Text = "<" + string.Join(", ", parameters) + ">";
			existing = result["items"] as JArray ?? new JArray();
			if (!popup.IsOpen && parameterKey != identity) {
				parameterKey = identity;
				choices.Clear();
				samples.Clear();
				acceptedArguments = "";
				fields.Children.Clear();
				inputs.Clear();
				foreach (var parameter in parameters) {
					fields.Children.Add(new TextBlock { Text = parameter, Margin = new Thickness(0, 4, 0, 2) });
					var input = new TextBox { MinWidth = 240, MinHeight = 23 };
					input.SetBinding(BackgroundProperty, new System.Windows.Data.Binding("Background") { Source = this });
					input.SetBinding(ForegroundProperty, new System.Windows.Data.Binding("Foreground") { Source = this });
					inputs.Add(input);
					fields.Children.Add(input);
				}
			}
			var supplied = (string?)result["arguments"] ?? "";
			if (!popup.IsOpen && supplied != acceptedArguments) {
				acceptedArguments = supplied;
				var parts = SplitArguments(supplied);
				for (int index = 0; index < inputs.Count; ++index) {
					inputs[index].Text = index < parts.Count ? parts[index] : "";
				}
			}
			var display = (string?)result["label"] ?? "";
			if (display.Length != 0 && supplied.Length != 0 && string.IsNullOrEmpty((string?)result["error"])) {
				samples[supplied] = display;
			}
			RebuildMenu();
		}

		internal static string JoinArguments(List<string> values) {
			int count = values.Count;
			while (count > 0 && string.IsNullOrWhiteSpace(values[count - 1])) {
				--count;
			}
			return string.Join(", ", values.GetRange(0, count));
		}

		internal static List<string> SplitArguments(string text) {
			var result = new List<string>();
			int depth = 0;
			int start = 0;
			char quote = '\0';
			for (int index = 0; index < text.Length; ++index) {
				char value = text[index];
				if (quote != '\0') {
					if (value == '\\') {
						++index;
					}
					else if (value == quote) {
						quote = '\0';
					}
					continue;
				}
				if (value == '\'' || value == '"') {
					quote = value;
				}
				else if (value == '<' || value == '(' || value == '[' || value == '{') {
					++depth;
				}
				else if (value == '>' || value == ')' || value == ']' || value == '}') {
					--depth;
				}
				else if (value == ',' && depth == 0) {
					result.Add(text.Substring(start, index - start).Trim());
					start = index + 1;
				}
			}
			if (text.Length != 0) {
				result.Add(text.Substring(start).Trim());
			}
			return result;
		}

		private void RebuildMenu() {
			menu.Items.Clear();
			menu.Items.Add(new MenuItem { Header = "Instantiations", IsEnabled = false });
			if (choices.Count == 0 && samples.Count == 0) {
				menu.Items.Add(new MenuItem { Header = "(None)", IsEnabled = false });
			}
			foreach (var sample in samples) {
				var text = sample.Key;
				var item = new MenuItem { Header = sample.Value };
				item.Click += (_, __) => {
					Arguments.Text = text;
					Apply?.Invoke();
				};
				menu.Items.Add(item);
			}
			foreach (var entry in choices) {
				var key = entry.Key;
				var item = new MenuItem { Header = entry.Value };
				item.Click += (_, __) => Select?.Invoke(key);
				menu.Items.Add(item);
			}
			menu.Items.Add(new Separator());
			var all = new MenuItem { Header = "Add All Existing Instantiations", IsEnabled = existing.Count != 0 };
			all.Click += (_, __) => {
				foreach (var entry in existing) {
					var key = (string?)entry["key"] ?? "";
					choices[key] = (string?)entry["label"] ?? key;
				}
				RebuildMenu();
			};
			menu.Items.Add(all);
			var clear = new MenuItem { Header = "Clear All Instantiations" };
			clear.Click += (_, __) => {
				choices.Clear();
				samples.Clear();
				RebuildMenu();
				Clear?.Invoke();
			};
			menu.Items.Add(clear);
		}
	}
}
