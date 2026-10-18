using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
using Newtonsoft.Json.Linq;
using Rutile.VisualStudio;

internal class TemplateArgumentsTests {
	[STAThread]
	public static int Main() {
		if (TemplateContextResponse.Declaration(new JArray(), 4, 4) != null) {
			throw new Exception("Pending response accepted");
		}
		var response = JObject.Parse("{\"template\":{\"name\":\"Box\"}}");
		if (TemplateContextResponse.Declaration(response, 4, 5) != null) {
			throw new Exception("Stale snapshot accepted");
		}
		if ((string)TemplateContextResponse.Declaration(response, 4, 4)!["name"]! != "Box") {
			throw new Exception("Current declaration lost");
		}
		foreach (double size in new[] { 11.0, 16.0, 24.0, 36.0 }) {
			var row = TemplateArgumentControls.CreateRow();
			TextElement.SetFontSize(row, size);
			row.Children.Add(new TextBlock { Text = "Box <", FontSize = size });
			var input = TemplateArgumentControls.CreateInput();
			input.Text = "i32";
			input.FontSize = size;
			row.Children.Add(input);
			row.Children.Add(new TextBlock { Text = ">", FontSize = size });
			row.Measure(new Size(double.PositiveInfinity, double.PositiveInfinity));
			row.Arrange(new Rect(row.DesiredSize));
			if (input.ActualHeight < input.DesiredSize.Height || row.ActualHeight < input.ActualHeight) {
				throw new Exception("Production input clipped");
			}
			var canvas = new Canvas { Width = 0, Height = 0, ClipToBounds = false };
			canvas.Children.Add(row);
			var geometry = new TemplateAdornmentGeometry(row.ActualHeight, size * .8);
			Canvas.SetTop(row, geometry.CanvasTop);
			if (geometry.TopSpace < row.ActualHeight || Math.Abs(geometry.CanvasTop + size * .8 + row.ActualHeight + 4) > .001) {
				throw new Exception("Row overlaps source line");
			}
			if (canvas.Width != 0 || canvas.ClipToBounds || Canvas.GetLeft(row) > 0) {
				throw new Exception("Adornment shifts or clips source");
			}
		}
		Console.WriteLine("Template arguments WPF measurement, pending response and snapshot checks: PASS");
		return 0;
	}
}
