using System.ComponentModel.Composition;
using System.Windows.Media;
using Microsoft.VisualStudio.Text.Classification;
using Microsoft.VisualStudio.Utilities;

namespace Rutile.VisualStudio {
	internal static class RutileClassificationDefinitions {
		[Export(typeof(ClassificationTypeDefinition)), Name(RutileClassification.Keyword), BaseDefinition("keyword")]
		internal static ClassificationTypeDefinition KeywordDefinition = null!;
		[Export(typeof(ClassificationTypeDefinition)), Name(RutileClassification.Control), BaseDefinition("keyword")]
		internal static ClassificationTypeDefinition ControlDefinition = null!;
		[Export(typeof(ClassificationTypeDefinition)), Name(RutileClassification.Type), BaseDefinition("identifier")]
		internal static ClassificationTypeDefinition TypeDefinition = null!;
		[Export(typeof(ClassificationTypeDefinition)), Name(RutileClassification.Function), BaseDefinition("identifier")]
		internal static ClassificationTypeDefinition FunctionDefinition = null!;
		[Export(typeof(ClassificationTypeDefinition)), Name(RutileClassification.Namespace), BaseDefinition("identifier")]
		internal static ClassificationTypeDefinition NamespaceDefinition = null!;
		[Export(typeof(ClassificationTypeDefinition)), Name(RutileClassification.Attribute), BaseDefinition("identifier")]
		internal static ClassificationTypeDefinition AttributeDefinition = null!;
	}

	[Export(typeof(EditorFormatDefinition)), Name(RutileClassification.Keyword), ClassificationType(ClassificationTypeNames = RutileClassification.Keyword), UserVisible(true), Order(After = Priority.Default, Before = Priority.High)]
	internal sealed class RutileKeywordFormat : ClassificationFormatDefinition {
		public RutileKeywordFormat() {
			DisplayName = "Rutile keyword";
			ForegroundColor = Color.FromRgb(86, 156, 214);
		}
	}

	[Export(typeof(EditorFormatDefinition)), Name(RutileClassification.Control), ClassificationType(ClassificationTypeNames = RutileClassification.Control), UserVisible(true), Order(After = Priority.Default, Before = Priority.High)]
	internal sealed class RutileControlFormat : ClassificationFormatDefinition {
		public RutileControlFormat() {
			DisplayName = "Rutile control keyword";
			ForegroundColor = Color.FromRgb(197, 134, 192);
		}
	}

	[Export(typeof(EditorFormatDefinition)), Name(RutileClassification.Type), ClassificationType(ClassificationTypeNames = RutileClassification.Type), UserVisible(true), Order(After = Priority.Default, Before = Priority.High)]
	internal sealed class RutileTypeFormat : ClassificationFormatDefinition {
		public RutileTypeFormat() {
			DisplayName = "Rutile type";
			ForegroundColor = Color.FromRgb(78, 201, 176);
		}
	}

	[Export(typeof(EditorFormatDefinition)), Name(RutileClassification.Function), ClassificationType(ClassificationTypeNames = RutileClassification.Function), UserVisible(true), Order(After = Priority.Default, Before = Priority.High)]
	internal sealed class RutileFunctionFormat : ClassificationFormatDefinition {
		public RutileFunctionFormat() {
			DisplayName = "Rutile function and enum value";
			ForegroundColor = Color.FromRgb(220, 220, 170);
		}
	}

	[Export(typeof(EditorFormatDefinition)), Name(RutileClassification.Namespace), ClassificationType(ClassificationTypeNames = RutileClassification.Namespace), UserVisible(true), Order(After = Priority.Default, Before = Priority.High)]
	internal sealed class RutileNamespaceFormat : ClassificationFormatDefinition {
		public RutileNamespaceFormat() {
			DisplayName = "Rutile namespace";
			ForegroundColor = Color.FromRgb(78, 201, 176);
		}
	}

	[Export(typeof(EditorFormatDefinition)), Name(RutileClassification.Attribute), ClassificationType(ClassificationTypeNames = RutileClassification.Attribute), UserVisible(true), Order(After = Priority.Default, Before = Priority.High)]
	internal sealed class RutileAttributeFormat : ClassificationFormatDefinition {
		public RutileAttributeFormat() {
			DisplayName = "Rutile attribute";
			ForegroundColor = Color.FromRgb(144, 144, 144);
		}
	}
}
