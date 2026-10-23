using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Windows;
using System.Windows.Controls;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using Rutile.VisualStudio;

public static partial class EditorObservations {
	public static string TemplateResponses() {
		var response = JObject.Parse("{ 'template': { 'name': 'Box' } }");
		return JsonConvert.SerializeObject(new {
			pending = TemplateContextResponse.Declaration(new JArray(), 4, 4),
			stale = TemplateContextResponse.Declaration(response, 4, 5),
			current = TemplateContextResponse.Declaration(response, 4, 4),
			nested = TemplateArgumentWidget.SplitArguments("Pair<i32, u08>, default_delete<Pair<i32, u08>>"),
			trailing_default = TemplateArgumentWidget.JoinArguments(new List<string> { "i32", "" }),
			interior_default = TemplateArgumentWidget.JoinArguments(new List<string> { "", "i32" })
		});
	}

	public static string TemplateWidget() {
		var widget = new TemplateArgumentWidget();
		var declaration = JObject.Parse("{ 'name': 'Box', 'parameters': ['T'] }");
		widget.Update(declaration, JObject.Parse("{ 'items': [] }"), "file:///one.rt:0");
		widget.Measure(new Size(850, 180));
		widget.Arrange(new Rect(0, 0, 850, 180));
		var height = widget.DesiredSize.Height;
		var inputVisible = widget.Arguments.IsVisible;
		var menu = (ContextMenu)typeof(TemplateArgumentWidget).GetField("menu", BindingFlags.Instance | BindingFlags.NonPublic)!.GetValue(widget)!;
		widget.Update(declaration, JObject.Parse("{ 'items': [{ 'key': 'Box<i32>', 'label': 'Box<i32>' }] }"), "file:///one.rt:0");
		menu.Items.OfType<MenuItem>().Single(item => Equals(item.Header, "Add All Existing Instantiations")).RaiseEvent(new RoutedEventArgs(MenuItem.ClickEvent));
		var added = menu.Items.OfType<MenuItem>().Select(item => item.Header?.ToString()).ToArray();
		var cleared = false;
		widget.Clear += () => cleared = true;
		menu.Items.OfType<MenuItem>().Single(item => Equals(item.Header, "Clear All Instantiations")).RaiseEvent(new RoutedEventArgs(MenuItem.ClickEvent));
		var afterClear = menu.Items.OfType<MenuItem>().Select(item => item.Header?.ToString()).ToArray();
		declaration = JObject.Parse("{ 'name': 'Box', 'parameters': ['T', 'D'] }");
		widget.Update(declaration, JObject.Parse("{ 'items': [], 'arguments': 'i32', 'label': 'Box<i32>' }"), "file:///one.rt:100");
		var inputs = (List<TextBox>)typeof(TemplateArgumentWidget).GetField("inputs", BindingFlags.Instance | BindingFlags.NonPublic)!.GetValue(widget)!;
		var sample = inputs.Select(input => input.Text).ToArray();
		widget.Update(declaration, JObject.Parse("{ 'items': [] }"), "file:///two.rt:100");
		return JsonConvert.SerializeObject(new {
			height_pixels = (int)Math.Ceiling(height), input_visible = inputVisible, added, clear_event = cleared, after_clear = afterClear,
			sample, next_declaration = inputs.Select(input => input.Text).ToArray(),
			next_menu = menu.Items.OfType<MenuItem>().Select(item => item.Header?.ToString()).ToArray()
		});
	}
}
