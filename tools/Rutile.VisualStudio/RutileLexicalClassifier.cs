using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Threading;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Classification;
using Microsoft.VisualStudio.Utilities;

namespace Rutile.VisualStudio {
	[Export(typeof(IClassifierProvider)), ContentType(RutileContentType.Name)]
	internal sealed class RutileLexicalClassifierProvider : IClassifierProvider {
		[Import]
		internal IClassificationTypeRegistryService Types = null!;
		[Import]
		internal ITextDocumentFactoryService Documents = null!;

		public IClassifier GetClassifier(ITextBuffer buffer) {
			return buffer.Properties.GetOrCreateSingletonProperty(() => {
				SemanticBufferColors? semantic = null;
				if (Documents.TryGetTextDocument(buffer, out var document)) {
					semantic = RutileSemanticStore.Instance.Register(buffer, document.FilePath);
				}
				return new RutileLexicalClassifier(buffer, Types, semantic);
			});
		}
	}

	internal sealed class RutileLexicalClassifier : IClassifier {
		private readonly IClassificationTypeRegistryService types;
		private sealed class CachedSpans {
			internal readonly ITextSnapshot Snapshot;
			internal readonly IReadOnlyList<LexicalSpan> Spans;

			internal CachedSpans(ITextSnapshot snapshot) {
				Snapshot = snapshot;
				Spans = RutileLexicalScanner.Scan(snapshot.GetText());
			}
		}

		private CachedSpans? cache;
		private readonly SemanticBufferColors? semantic;
		private readonly ITextBuffer buffer;

		public event EventHandler<ClassificationChangedEventArgs>? ClassificationChanged;

		internal RutileLexicalClassifier(ITextBuffer buffer, IClassificationTypeRegistryService types, SemanticBufferColors? semantic) {
			this.types = types;
			this.buffer = buffer;
			this.semantic = semantic;
			if (semantic != null) {
				semantic.Changed += OnSemanticChanged;
			}
			buffer.Changed += OnChanged;
		}

		private void OnSemanticChanged(object sender, EventArgs args) {
			var current = buffer.CurrentSnapshot;
			ClassificationChanged?.Invoke(this, new ClassificationChangedEventArgs(new SnapshotSpan(current, 0, current.Length)));
		}

		private void OnChanged(object sender, TextContentChangedEventArgs args) {
			Interlocked.Exchange(ref cache, null);
			semantic?.Translate(args);
			ClassificationChanged?.Invoke(this, new ClassificationChangedEventArgs(new SnapshotSpan(args.After, 0, args.After.Length)));
		}

		public IList<ClassificationSpan> GetClassificationSpans(SnapshotSpan requested) {
			var current = Volatile.Read(ref cache);
			if (current?.Snapshot != requested.Snapshot) {
				current = new CachedSpans(requested.Snapshot);
				Interlocked.Exchange(ref cache, current);
			}
			var result = new List<ClassificationSpan>();
			foreach (var item in current.Spans) {
				var span = new SnapshotSpan(requested.Snapshot, item.Start, item.Length);
				if (!span.IntersectsWith(requested)) {
					continue;
				}
				var type = Classification(item.Kind);
				if (type != null) {
					result.Add(new ClassificationSpan(span, type));
				}
			}
			if (semantic != null) {
				foreach (var item in semantic.Current(requested.Snapshot)) {
					if (item.Span.IntersectsWith(requested) && !RutileLexicalScanner.MasksSemantic(current.Spans, item.Span.Start.Position, item.Span.Length)) {
						var type = types.GetClassificationType(item.Classification);
						if (type != null) {
							result.Add(new ClassificationSpan(item.Span, type));
						}
					}
				}
			}
			return result;
		}

		private IClassificationType? Classification(LexicalKind kind) {
			switch (kind) {
				case LexicalKind.Keyword:
					return types.GetClassificationType(RutileClassification.Keyword);
				case LexicalKind.ControlKeyword:
					return types.GetClassificationType(RutileClassification.Control);
				case LexicalKind.Comment:
					return types.GetClassificationType("comment");
				case LexicalKind.String:
					return types.GetClassificationType("string");
				case LexicalKind.Number:
					return types.GetClassificationType("number");
				case LexicalKind.Operator:
					return types.GetClassificationType("operator");
				default:
					return null;
			}
		}
	}
}
