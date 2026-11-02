using System;
using System.ComponentModel.Composition;
using System.IO;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.Utilities;

namespace Rutile.VisualStudio {
	internal static class RutileDocumentProject {
		internal static event EventHandler? Changed;
		internal static string? ActivePath { get; private set; }

		internal static string? FindRoot(string? path) {
			if (string.IsNullOrWhiteSpace(path)) {
				return null;
			}
			var directory = Path.GetDirectoryName(Path.GetFullPath(path));
			while (directory != null) {
				if (File.Exists(Path.Combine(directory, "kiln.rt"))) {
					return directory;
				}
				directory = Path.GetDirectoryName(directory);
			}
			return null;
		}

		internal static void Activate(string path) {
			if (string.Equals(ActivePath, path, StringComparison.OrdinalIgnoreCase)) {
				return;
			}
			ActivePath = path;
			Changed?.Invoke(null, EventArgs.Empty);
		}

		internal static void Close(string path) {
			if (!string.Equals(ActivePath, path, StringComparison.OrdinalIgnoreCase)) {
				return;
			}
			ActivePath = null;
			Changed?.Invoke(null, EventArgs.Empty);
		}
	}

	[Export(typeof(IWpfTextViewCreationListener)), ContentType(RutileContentType.Name), TextViewRole(PredefinedTextViewRoles.Document)]
	internal sealed class RutileDocumentFocus : IWpfTextViewCreationListener {
		[Import]
		internal ITextDocumentFactoryService Documents = null!;

		public void TextViewCreated(IWpfTextView view) {
			ITextDocument? tracked = null;
			EventHandler<TextDocumentEventArgs>? created = null;
			Action<ITextDocument> attach = document => {
				if (!ReferenceEquals(document.TextBuffer, view.TextBuffer)) {
					return;
				}
				tracked = document;
				Documents.TextDocumentCreated -= created;
				if (view.HasAggregateFocus || RutileDocumentProject.ActivePath == null) {
					RutileDocumentProject.Activate(document.FilePath);
				}
			};
			created = (_, args) => attach(args.TextDocument);
			EventHandler focus = (_, __) => {
				if (Documents.TryGetTextDocument(view.TextBuffer, out var document)) {
					attach(document);
				}
				if (tracked != null) {
					RutileDocumentProject.Activate(tracked.FilePath);
				}
			};
			Documents.TextDocumentCreated += created;
			view.GotAggregateFocus += focus;
			view.Closed += (_, __) => {
				Documents.TextDocumentCreated -= created;
				view.GotAggregateFocus -= focus;
				if (tracked != null) {
					RutileDocumentProject.Close(tracked.FilePath);
				}
			};
			if (Documents.TryGetTextDocument(view.TextBuffer, out var existing)) {
				attach(existing);
			}
		}
	}
}
