using System;
using System.Collections.Generic;

namespace Rutile.VisualStudio {
	internal enum LexicalKind {
		Comment,
		String,
		Number,
		Operator
	}

	internal readonly struct LexicalSpan {
		internal readonly int Start;
		internal readonly int Length;
		internal readonly LexicalKind Kind;

		internal LexicalSpan(int start, int length, LexicalKind kind) {
			Start = start;
			Length = length;
			Kind = kind;
		}
	}

	internal static class RutileLexicalScanner {
		internal static IReadOnlyList<LexicalSpan> Scan(string text) {
			var result = new List<LexicalSpan>();
			var offset = 0;
			while (offset < text.Length) {
				var start = offset;
				var current = text[offset++];
				if (current == '/' && offset < text.Length && text[offset] == '/') {
					while (offset < text.Length && text[offset] != '\n') {
						++offset;
					}
					result.Add(new LexicalSpan(start, offset - start, LexicalKind.Comment));
					continue;
				}
				if (current == '/' && offset < text.Length && text[offset] == '*') {
					++offset;
					while (offset < text.Length) {
						if (text[offset++] == '*' && offset < text.Length && text[offset] == '/') {
							++offset;
							break;
						}
					}
					result.Add(new LexicalSpan(start, offset - start, LexicalKind.Comment));
					continue;
				}
				if (current == '"' || current == '\'') {
					while (offset < text.Length) {
						var value = text[offset++];
						if (value == '\\' && offset < text.Length) {
							++offset;
							continue;
						}
						if (value == current || value == '\n' || value == '\r') {
							break;
						}
					}
					result.Add(new LexicalSpan(start, offset - start, LexicalKind.String));
					continue;
				}
				if (IdentifierStart(current)) {
					while (offset < text.Length && (IdentifierStart(text[offset]) || Digit(text[offset]))) {
						++offset;
					}
					continue;
				}
				if (Digit(current)) {
					while (offset < text.Length && Digit(text[offset])) {
						++offset;
					}
					if (offset + 1 < text.Length && text[offset] == '.' && Digit(text[offset + 1])) {
						++offset;
						while (offset < text.Length && Digit(text[offset])) {
							++offset;
						}
					}
					if (offset < text.Length && (text[offset] == 'e' || text[offset] == 'E')) {
						++offset;
						if (offset < text.Length && (text[offset] == '+' || text[offset] == '-')) {
							++offset;
						}
						while (offset < text.Length && Digit(text[offset])) {
							++offset;
						}
					}
					result.Add(new LexicalSpan(start, offset - start, LexicalKind.Number));
					continue;
				}
				if ("+-*/%=!<>&|^~@$".IndexOf(current) >= 0) {
					result.Add(new LexicalSpan(start, 1, LexicalKind.Operator));
				}
			}
			return result;
		}

		private static bool IdentifierStart(char value) {
			return value >= 'a' && value <= 'z' || value >= 'A' && value <= 'Z' || value == '_';
		}

		private static bool Digit(char value) {
			return value >= '0' && value <= '9';
		}
	}
}
