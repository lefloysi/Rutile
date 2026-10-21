using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Runtime.CompilerServices;
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

		private readonly ConditionalWeakTable<ITextBuffer, RutileLexicalClassifier> waiting = new ConditionalWeakTable<ITextBuffer, RutileLexicalClassifier>();
		private int subscribed;

		public IClassifier GetClassifier(ITextBuffer buffer) {
			if (Interlocked.CompareExchange(ref subscribed, 1, 0) == 0) {
				Documents.TextDocumentCreated += OnDocumentCreated;
			}
			return buffer.Properties.GetOrCreateSingletonProperty(() => {
				var classifier = new RutileLexicalClassifier(buffer, Types, null);
				waiting.Add(buffer, classifier);
				if (Documents.TryGetTextDocument(buffer, out var document)) {
					Attach(document);
				}
				return classifier;
			});
		}

		private void OnDocumentCreated(object sender, TextDocumentEventArgs args) {
			Attach(args.TextDocument);
		}

		private void Attach(ITextDocument document) {
			if (waiting.TryGetValue(document.TextBuffer, out var classifier)) {
				classifier.AttachSemantic(RutileSemanticStore.Instance.Register(document.TextBuffer, document.FilePath));
				waiting.Remove(document.TextBuffer);
			}
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
		private SemanticBufferColors? semantic;
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

		internal void AttachSemantic(SemanticBufferColors state) {
			if (Interlocked.CompareExchange(ref semantic, state, null) != null) {
				return;
			}
			state.Changed += OnSemanticChanged;
			OnSemanticChanged(this, EventArgs.Empty);
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
			var result = new List<ClassificationSpan>();
			var authoritative = false;
			var semanticSpans = semantic?.Current(requested.Snapshot, out authoritative);
			IReadOnlyList<LexicalSpan> fallback = Array.Empty<LexicalSpan>();
			if (!authoritative) {
				var current = Volatile.Read(ref cache);
				if (current?.Snapshot != requested.Snapshot) {
					current = new CachedSpans(requested.Snapshot);
					Interlocked.Exchange(ref cache, current);
				}
				fallback = current.Spans;
			}
			foreach (var item in fallback) {
				var span = new SnapshotSpan(requested.Snapshot, item.Start, item.Length);
				if (!span.IntersectsWith(requested)) {
					continue;
				}
				var overlapsPublished = false;
				if (semanticSpans != null) {
					foreach (var published in semanticSpans) {
						if (published.Span.IntersectsWith(span)) {
							overlapsPublished = true;
							break;
						}
					}
				}
				if (overlapsPublished) {
					continue;
				}
				var type = Classification(item.Kind);
				if (type != null) {
					result.Add(new ClassificationSpan(span, type));
				}
			}
			if (semanticSpans != null) {
				foreach (var item in semanticSpans) {
					if (item.Span.IntersectsWith(requested)) {
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
