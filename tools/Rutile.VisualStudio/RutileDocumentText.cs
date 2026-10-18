using Newtonsoft.Json.Linq;

namespace Rutile.VisualStudio {
	internal static class RutileDocumentText {
		internal static string? Apply(string text, JArray? changes) {
			if (changes == null) {
				return null;
			}
			foreach (var change in changes) {
				var inserted = (string?)change["text"];
				if (inserted == null) {
					return null;
				}
				var range = change["range"];
				if (range == null) {
					text = inserted;
					continue;
				}
				var start = Offset(text, range["start"]);
				var end = Offset(text, range["end"]);
				if (start < 0 || end < start) {
					return null;
				}
				text = text.Substring(0, start) + inserted + text.Substring(end);
			}
			return text;
		}

		private static int Offset(string text, JToken? position) {
			var line = (int?)position?["line"] ?? -1;
			var character = (int?)position?["character"] ?? -1;
			if (line < 0 || character < 0) {
				return -1;
			}
			var start = 0;
			while (line != 0) {
				if (start == text.Length) {
					return -1;
				}
				var value = text[start++];
				if (value == '\r' || value == '\n') {
					if (value == '\r' && start < text.Length && text[start] == '\n') {
						++start;
					}
					--line;
				}
			}
			var end = start;
			while (end < text.Length && text[end] != '\r' && text[end] != '\n') {
				++end;
			}
			return character <= end - start ? start + character : -1;
		}
	}
}
