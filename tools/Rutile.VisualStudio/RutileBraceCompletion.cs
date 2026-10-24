using System;
using System.ComponentModel.Composition;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.BraceCompletion;
using Microsoft.VisualStudio.Text.Classification;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.Utilities;

namespace Rutile.VisualStudio {
	[Export(typeof(IBraceCompletionDefaultProvider)), ContentType(RutileContentType.Name)]
	[BracePair('(', ')'), BracePair('[', ']'), BracePair('{', '}')]
	[BracePair('"', '"'), BracePair('\'', '\'')]
	internal sealed class RutileBraceCompletionDefaults : IBraceCompletionDefaultProvider {
	}

	// The editor owns insertion, paired deletion, overtype, caret tracking and undo.
	[Export(typeof(IBraceCompletionContextProvider)), ContentType(RutileContentType.Name)]
	[BracePair('(', ')'), BracePair('[', ']'), BracePair('{', '}')]
	[BracePair('"', '"'), BracePair('\'', '\'')]
	internal sealed class RutileBraceCompletionProvider : IBraceCompletionContextProvider {
		[Import]
		internal IClassifierAggregatorService Classifiers = null!;

		public bool TryCreateContext(ITextView textView, SnapshotPoint openingPoint, char openingBrace, char closingBrace, out IBraceCompletionContext context) {
			context = null!;
			if (openingPoint.Position < openingPoint.Snapshot.Length) {
				var point = new SnapshotSpan(openingPoint, 1);
				foreach (var span in Classifiers.GetClassifier(openingPoint.Snapshot.TextBuffer).GetClassificationSpans(point)) {
					if ((span.ClassificationType.IsOfType("comment") || span.ClassificationType.IsOfType("string")) && span.Span.Start.Position < openingPoint.Position) {
						return false;
					}
				}
			}
			var prefix = openingPoint.Snapshot.GetText(0, openingPoint.Position);
			if (!CanStart(prefix, openingBrace, closingBrace)) {
				return false;
			}
			context = new RutileBraceCompletionContext();
			return true;
		}

		internal static bool CanStart(string prefix, char opening, char closing) {
			if (!(opening == '(' && closing == ')' || opening == '[' && closing == ']' || opening == '{' && closing == '}' || opening == '"' && closing == '"' || opening == '\'' && closing == '\'')) {
				return false;
			}
			var spans = RutileLexicalScanner.Scan(prefix);
			if (spans.Count == 0) {
				return true;
			}
			var last = spans[spans.Count - 1];
			if (last.Start + last.Length != prefix.Length) {
				return true;
			}
			if (last.Kind == LexicalKind.Comment) {
				return prefix[last.Start + 1] == '*' && prefix.EndsWith("*/", StringComparison.Ordinal);
			}
			if (last.Kind != LexicalKind.String) {
				return true;
			}
			if (last.Length >= 3 && prefix.Substring(last.Start, 3) == "\"\"\"") {
				return last.Length >= 6 && prefix.EndsWith("\"\"\"", StringComparison.Ordinal);
			}
			var end = prefix.Length - 1;
			if (prefix[end] == '\r' || prefix[end] == '\n') {
				return true;
			}
			return last.Length > 1 && prefix[end] == prefix[last.Start] && !Escaped(prefix, end);
		}

		internal static bool Escaped(string text, int position) {
			var slashes = 0;
			while (position > 0 && text[--position] == '\\') {
				++slashes;
			}
			return slashes % 2 != 0;
		}
	}

	internal sealed class RutileBraceCompletionContext : IBraceCompletionContext {
		public void Start(IBraceCompletionSession session) {
		}

		public void Finish(IBraceCompletionSession session) {
		}

		public void OnReturn(IBraceCompletionSession session) {
			if (session.OpeningBrace != '{') {
				return;
			}
			var snapshot = session.SubjectBuffer.CurrentSnapshot;
			var opening = session.OpeningPoint.GetPosition(snapshot);
			var closing = session.ClosingPoint.GetPosition(snapshot) - 1;
			if (opening < 0 || closing <= opening || closing >= snapshot.Length || snapshot[opening] != '{' || snapshot[closing] != '}') {
				return;
			}
			var contents = snapshot.GetText(opening + 1, closing - opening - 1);
			if (!string.IsNullOrWhiteSpace(contents)) {
				return;
			}
			var line = snapshot.GetLineFromPosition(opening);
			var text = line.GetText();
			var length = 0;
			while (length < text.Length && (text[length] == ' ' || text[length] == '\t')) {
				++length;
			}
			var indent = text.Substring(0, length);
			var options = session.TextView.Options;
			var body = BodyIndent(indent, options.GetOptionValue(DefaultOptions.IndentSizeOptionId), options.GetOptionValue(DefaultOptions.TabSizeOptionId), options.GetOptionValue(DefaultOptions.ConvertTabsToSpacesOptionId));
			var newline = line.GetLineBreakText();
			if (newline.Length == 0) {
				newline = options.GetOptionValue(DefaultOptions.NewLineCharacterOptionId);
			}
			var replacement = newline + body + newline + indent;
			var updated = session.SubjectBuffer.Replace(new Span(opening + 1, contents.Length), replacement);
			session.TextView.Caret.MoveTo(new SnapshotPoint(updated, opening + 1 + newline.Length + body.Length));
			session.TextView.Caret.EnsureVisible();
		}

		internal static string BodyIndent(string indent, int size, int tabSize, bool spaces) {
			tabSize = Math.Max(1, tabSize);
			var column = 0;
			foreach (var value in indent) {
				column = value == '\t' ? column + tabSize - column % tabSize : column + 1;
			}
			var target = column + Math.Max(1, size);
			var result = indent;
			while (!spaces && column + tabSize - column % tabSize <= target) {
				result += '\t';
				column += tabSize - column % tabSize;
			}
			return result + new string(' ', target - column);
		}

		public bool AllowOverType(IBraceCompletionSession session) {
			if (session.ClosingBrace != '"' && session.ClosingBrace != '\'') {
				return true;
			}
			var caret = session.TextView.Caret.Position.BufferPosition;
			var prefix = caret.Snapshot.GetText(0, caret.Position);
			return !RutileBraceCompletionProvider.Escaped(prefix, prefix.Length);
		}
	}
}
