using System.ComponentModel.Composition;
using System.Windows.Media;
using Microsoft.VisualStudio.Text.Classification;
using Microsoft.VisualStudio.Utilities;

namespace vs_rtsl_ext
{
    internal static class RtslClassificationDefinitions
    {
        [Export(typeof(ClassificationTypeDefinition))]
        [Name(RtslClassificationNames.DeclarationKeyword)]
        [BaseDefinition("keyword")]
        internal static ClassificationTypeDefinition DeclarationKeyword;

        [Export(typeof(ClassificationTypeDefinition))]
        [Name(RtslClassificationNames.ControlFlowKeyword)]
        [BaseDefinition("keyword")]
        internal static ClassificationTypeDefinition ControlFlowKeyword;

        [Export(typeof(ClassificationTypeDefinition))]
        [Name(RtslClassificationNames.TypeName)]
        [BaseDefinition("identifier")]
        internal static ClassificationTypeDefinition TypeName;

        [Export(typeof(ClassificationTypeDefinition))]
        [Name(RtslClassificationNames.ResourceAccess)]
        [BaseDefinition("keyword")]
        internal static ClassificationTypeDefinition ResourceAccess;

        [Export(typeof(ClassificationTypeDefinition))]
        [Name(RtslClassificationNames.VaryingQualifier)]
        [BaseDefinition("keyword")]
        internal static ClassificationTypeDefinition VaryingQualifier;
    }

    [Export(typeof(EditorFormatDefinition))]
    [ClassificationType(ClassificationTypeNames = RtslClassificationNames.DeclarationKeyword)]
    [Name(RtslClassificationNames.DeclarationKeyword)]
    [UserVisible(true)]
    [Order(Before = Priority.Default)]
    internal sealed class RtslDeclarationKeywordFormat : ClassificationFormatDefinition
    {
        public RtslDeclarationKeywordFormat()
        {
            DisplayName = "RTSL Declaration Keyword";
            ForegroundColor = Color.FromRgb(86, 156, 214);
        }
    }

    [Export(typeof(EditorFormatDefinition))]
    [ClassificationType(ClassificationTypeNames = RtslClassificationNames.ControlFlowKeyword)]
    [Name(RtslClassificationNames.ControlFlowKeyword)]
    [UserVisible(true)]
    [Order(Before = Priority.Default)]
    internal sealed class RtslControlFlowKeywordFormat : ClassificationFormatDefinition
    {
        public RtslControlFlowKeywordFormat()
        {
            DisplayName = "RTSL Control Flow Keyword";
            ForegroundColor = Color.FromRgb(197, 134, 192);
        }
    }

    [Export(typeof(EditorFormatDefinition))]
    [ClassificationType(ClassificationTypeNames = RtslClassificationNames.TypeName)]
    [Name(RtslClassificationNames.TypeName)]
    [UserVisible(true)]
    [Order(Before = Priority.Default)]
    internal sealed class RtslTypeNameFormat : ClassificationFormatDefinition
    {
        public RtslTypeNameFormat()
        {
            DisplayName = "RTSL Type Name";
            ForegroundColor = Color.FromRgb(78, 201, 176);
        }
    }

    [Export(typeof(EditorFormatDefinition))]
    [ClassificationType(ClassificationTypeNames = RtslClassificationNames.ResourceAccess)]
    [Name(RtslClassificationNames.ResourceAccess)]
    [UserVisible(true)]
    [Order(Before = Priority.Default)]
    internal sealed class RtslResourceAccessFormat : ClassificationFormatDefinition
    {
        public RtslResourceAccessFormat()
        {
            DisplayName = "RTSL Resource Access";
            ForegroundColor = Color.FromRgb(215, 186, 125);
        }
    }

    [Export(typeof(EditorFormatDefinition))]
    [ClassificationType(ClassificationTypeNames = RtslClassificationNames.VaryingQualifier)]
    [Name(RtslClassificationNames.VaryingQualifier)]
    [UserVisible(true)]
    [Order(Before = Priority.Default)]
    internal sealed class RtslVaryingQualifierFormat : ClassificationFormatDefinition
    {
        public RtslVaryingQualifierFormat()
        {
            DisplayName = "RTSL Varying Qualifier";
            ForegroundColor = Color.FromRgb(220, 220, 170);
        }
    }
}
