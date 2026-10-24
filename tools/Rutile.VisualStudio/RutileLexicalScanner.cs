using System;
using System.Collections.Generic;

namespace Rutile.VisualStudio {
	internal enum LexicalKind {
		Comment,
		String,
		Number,
		Keyword,
		ControlKeyword,
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
					if (current == '"' && offset + 1 < text.Length && text[offset] == '"' && text[offset + 1] == '"') {
						offset += 2;
						while (offset < text.Length) {
							if (offset + 2 < text.Length && text[offset] == '"' && text[offset + 1] == '"' && text[offset + 2] == '"') {
								offset += 3;
								break;
							}
							++offset;
						}
						result.Add(new LexicalSpan(start, offset - start, LexicalKind.String));
						continue;
					}
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
					var keyword = KeywordKind(text.Substring(start, offset - start));
					if (keyword.HasValue) {
						result.Add(new LexicalSpan(start, offset - start, keyword.Value));
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

		// Reserved spellings: compiler/src/frontend/lexer.rt, Lexer::keyword_spelling.
		// Control range: language_lexical_type in compiler/src/language/server/semantic.rt.
		private static LexicalKind? KeywordKind(string spelling) {
			switch (spelling) {
				case "fn":
				case "using":
				case "const":
				case "discard":
				case "delete":
				case "new":
				case "implicit":
				case "nullptr":
				case "export":
				case "template":
				case "typename":
				case "class":
				case "var":
				case "struct":
				case "union":
				case "enum":
				case "namespace":
				case "virtual":
				case "override":
				case "final":
				case "sizeof":
					return LexicalKind.Keyword;
				case "return":
				case "try":
				case "catch":
				case "throw":
				case "noexcept":
				case "if":
				case "else":
				case "while":
				case "for":
				case "switch":
				case "case":
				case "default":
				case "break":
				case "continue":
					return LexicalKind.ControlKeyword;
				default:
					return null;
			}
		}

		private static bool IdentifierStart(char value) {
			return value >= 'a' && value <= 'z' || value >= 'A' && value <= 'Z' || value == '_';
		}

		private static bool Digit(char value) {
			return value >= '0' && value <= '9';
		}
	}
}
