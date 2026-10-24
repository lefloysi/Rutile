using System;
using System.Collections.Generic;
using Newtonsoft.Json.Linq;

namespace Rutile.VisualStudio {
	internal readonly struct SemanticColorSpan {
		internal readonly int Start;
		internal readonly int Length;
		internal readonly string Classification;

		internal SemanticColorSpan(int start, int length, string classification) {
			Start = start;
			Length = length;
			Classification = classification;
		}
	}

	internal static class RutileSemanticDecoder {
		internal static IReadOnlyList<SemanticColorSpan> Decode(string source, JArray data, string[] legend) {
			var result = new List<SemanticColorSpan>();
			if (data.Count % 5 != 0) {
				return result;
			}
			var lines = new List<int> { 0 };
			for (var offset = 0; offset < source.Length; ++offset) {
				if (source[offset] == '\r' || source[offset] == '\n') {
					if (source[offset] == '\r' && offset + 1 < source.Length && source[offset + 1] == '\n') {
						++offset;
					}
					lines.Add(offset + 1);
				}
			}
			long line = 0;
			long character = 0;
			for (var index = 0; index < data.Count; index += 5) {
				for (var item = 0; item < 5; ++item) {
					if (data[index + item].Type != JTokenType.Integer || (long)data[index + item] < 0 || (long)data[index + item] > int.MaxValue) {
						return Array.Empty<SemanticColorSpan>();
					}
				}
				var deltaLine = (int)data[index];
				line += deltaLine;
				character = deltaLine == 0 ? character + (int)data[index + 1] : (int)data[index + 1];
				var length = (int)data[index + 2];
				var kind = (int)data[index + 3];
				if (line >= lines.Count || kind >= legend.Length || length == 0) {
					continue;
				}
				var begin = lines[(int)line] + character;
				var end = line + 1 < lines.Count ? lines[(int)line + 1] : source.Length;
				while (end > lines[(int)line] && (source[end - 1] == '\r' || source[end - 1] == '\n')) {
					--end;
				}
				var classification = Classification(legend[kind]);
				if (classification != null && begin <= end && length <= end - begin) {
					result.Add(new SemanticColorSpan((int)begin, length, classification));
				}
			}
			return result;
		}

		private static string? Classification(string token) {
			switch (token) {
				case "type":
				case "class":
				case "struct":
				case "enum":
				case "interface":
				case "typeParameter":
					return RutileClassification.Type;
				case "decorator":
					return RutileClassification.Attribute;
				case "namespace":
					return RutileClassification.Namespace;
				case "function":
				case "method":
				case "enumMember":
					return RutileClassification.Function;
				case "keyword":
					return RutileClassification.Keyword;
				case "controlKeyword":
					return RutileClassification.Control;
				case "comment":
				case "string":
				case "number":
				case "operator":
					return token;
				default:
					return null;
			}
		}
	}
}
