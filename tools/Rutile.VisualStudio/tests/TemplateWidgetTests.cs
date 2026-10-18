using System;
using System.IO;
using System.Reflection;
using System.Windows.Controls.Primitives;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using Newtonsoft.Json.Linq;
using Rutile.VisualStudio;
internal class TemplateWidgetTests {
	[STAThread]
	public static int Main(string[] args) {
		var widget = new TemplateArgumentWidget { Background = new SolidColorBrush(Color.FromRgb(37,37,38)), Foreground = Brushes.Gainsboro };
		widget.Update(JObject.Parse("{\"name\":\"Box\",\"parameters\":[\"T\"]}"), JObject.Parse("{\"items\":[]}"), "file:///one.rt:0");
		var row = new StackPanel { Orientation = Orientation.Horizontal, Margin = new Thickness(20) };
		row.Children.Add(new TextBlock { Text = "template<typename T>", Foreground = Brushes.Gainsboro, FontFamily = new FontFamily("Consolas"), VerticalAlignment = VerticalAlignment.Center });
		row.Children.Add(widget);
		var root = new StackPanel { Background = widget.Background };
		root.Children.Add(row);
		root.Children.Add(new TextBlock { Text = "struct Box {\n    T value;\n};", Foreground = Brushes.Gainsboro, FontFamily = new FontFamily("Consolas"), Margin = new Thickness(20,0,0,20) });
		root.Measure(new Size(850, 180));
		root.Arrange(new Rect(0,0,850,180));
		if (widget.DesiredSize.Height > 32) {
			throw new Exception("Widget is not compact");
		}
		if (widget.Arguments.IsVisible) {
			throw new Exception("Input must remain inside pencil popup");
		}
		var image = new RenderTargetBitmap(850,180,96,96,PixelFormats.Pbgra32);
		image.Render(root);
		var png = new PngBitmapEncoder();
		png.Frames.Add(BitmapFrame.Create(image));
		using (var file = File.Create(args[0])) {
			png.Save(file);
		}
		widget.Update(JObject.Parse("{\"name\":\"Box\",\"parameters\":[\"T\"]}"), JObject.Parse("{\"items\":[{\"key\":\"Box<i32>\",\"label\":\"Box<i32>\"}]}"), "file:///one.rt:0");
		var menu = (ContextMenu)typeof(TemplateArgumentWidget).GetField("menu", BindingFlags.Instance | BindingFlags.NonPublic)!.GetValue(widget)!;
		((MenuItem)menu.Items[3]).RaiseEvent(new RoutedEventArgs(MenuItem.ClickEvent));
		if (((MenuItem)menu.Items[1]).Header.ToString() != "Box<i32>") {
			throw new Exception("Add existing did not add actual context");
		}
		bool cleared = false;
		widget.Clear += () => {
			cleared = true;
		};
		((MenuItem)menu.Items[4]).RaiseEvent(new RoutedEventArgs(MenuItem.ClickEvent));
		if (!cleared || ((MenuItem)menu.Items[1]).Header.ToString() != "(None)") {
			throw new Exception("Clear did not remove contexts and reset selection");
		}
		var popup = (Popup)typeof(TemplateArgumentWidget).GetField("popup", BindingFlags.Instance | BindingFlags.NonPublic)!.GetValue(widget)!;
		widget.RaiseEvent(new RoutedEventArgs(FrameworkElement.LoadedEvent));
		var panel = (FrameworkElement)popup.Child;
		((Border)panel).Background = widget.Background;
		panel.SetValue(System.Windows.Documents.TextElement.ForegroundProperty, widget.Foreground);
		Save(panel, Path.ChangeExtension(args[0], ".edit.png"), 430, 190);
		menu.Background = widget.Background;
		menu.Foreground = widget.Foreground;
		Save(menu, Path.ChangeExtension(args[0], ".menu.png"), 380, 190);
		if (TemplateArgumentWidget.JoinArguments(new System.Collections.Generic.List<string> { "i32", "" }) != "i32") {
			throw new Exception("Trailing default argument was not omitted");
		}
		if (TemplateArgumentWidget.JoinArguments(new System.Collections.Generic.List<string> { "", "i32" }) != ", i32") {
			throw new Exception("Interior missing argument silently changed positions");
		}
		var arguments = TemplateArgumentWidget.SplitArguments("Pair<i32, u08>, default_delete<Pair<i32, u08>>");
		if (arguments.Count != 2 || arguments[0] != "Pair<i32, u08>") {
			throw new Exception("Nested argument edit split incorrectly");
		}
		var declaration = JObject.Parse("{\"name\":\"Box\",\"parameters\":[\"T\",\"D\"]}");
		widget.Update(declaration, JObject.Parse("{\"items\":[],\"arguments\":\"i32\",\"label\":\"Box<i32>\"}"), "file:///one.rt:100");
		var inputs = (System.Collections.Generic.List<TextBox>)typeof(TemplateArgumentWidget).GetField("inputs", BindingFlags.Instance | BindingFlags.NonPublic)!.GetValue(widget)!;
		if (inputs[0].Text != "i32" || inputs[1].Text != "" || ((MenuItem)menu.Items[1]).Header.ToString() != "Box<i32>") {
			throw new Exception("Accepted sample not editable/selectable");
		}
		widget.Update(declaration, JObject.Parse("{\"items\":[]}"), "file:///two.rt:100");
		if (((MenuItem)menu.Items[1]).Header.ToString() != "(None)" || inputs[0].Text != "") {
			throw new Exception("Sample leaked across declaration identity");
		}
		Console.WriteLine("Compact production widget render and hidden input: PASS");
		return 0;
	}
	private static void Save(FrameworkElement control, string path, int width, int height) {
		control.Measure(new Size(width, height));
		control.Arrange(new Rect(0,0,width,height));
		var bitmap = new RenderTargetBitmap(width,height,96,96,PixelFormats.Pbgra32);
		bitmap.Render(control);
		var encoder = new PngBitmapEncoder();
		encoder.Frames.Add(BitmapFrame.Create(bitmap));
		using (var file = File.Create(path)) {
			encoder.Save(file);
		}
	}

}
