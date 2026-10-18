using System;
using Newtonsoft.Json.Linq;
using System.Windows;
using System.Windows.Controls;

namespace Rutile.VisualStudio {
	internal static class TemplateArgumentControls {
		internal static StackPanel CreateRow() {
			return new StackPanel { Orientation = Orientation.Horizontal, MinHeight = 26 };
		}

		internal static TextBox CreateInput() {
			return new TextBox { Width = 260, MinHeight = 23, VerticalContentAlignment = VerticalAlignment.Center };
		}
	}

	internal readonly struct TemplateAdornmentGeometry {
		internal double TopSpace { get; }
		internal double CanvasTop { get; }

		internal TemplateAdornmentGeometry(double rowHeight, double textBaseline) {
			if (rowHeight < 0 || textBaseline < 0 || double.IsNaN(rowHeight) || double.IsNaN(textBaseline)) {
				throw new ArgumentOutOfRangeException(nameof(rowHeight));
			}
			TopSpace = rowHeight + 4;
			CanvasTop = -textBaseline - TopSpace;
		}
	}

	internal static class TemplateContextResponse {
		internal static JObject? Declaration(JToken? response, int requestedVersion, int currentVersion) {
			if (requestedVersion != currentVersion || !(response is JObject result)) {
				return null;
			}
			return result["template"] as JObject;
		}
	}
}
