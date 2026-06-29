using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Classification;
using Microsoft.VisualStudio.Utilities;

namespace vs_rtsl_ext
{
    [Export(typeof(IClassifierProvider))]
    [ContentType(RtslContentType.Name)]
    internal sealed class RtslClassifierProvider : IClassifierProvider
    {
        [Import]
        internal IClassificationTypeRegistryService ClassificationRegistry = null;

        public IClassifier GetClassifier(ITextBuffer buffer)
        {
            RtslClassificationTypes types = new RtslClassificationTypes(
                GetClassificationType(RtslClassificationNames.DeclarationKeyword, "keyword"),
                GetClassificationType(RtslClassificationNames.ControlFlowKeyword, "keyword"),
                GetClassificationType(RtslClassificationNames.ResourceAccess, "keyword"),
                GetClassificationType(RtslClassificationNames.TypeName, "class name", "identifier"),
                GetClassificationType(RtslClassificationNames.VaryingQualifier, "keyword"),
                GetClassificationType("comment"),
                GetClassificationType("string"),
                GetClassificationType("number"));

            return buffer.Properties.GetOrCreateSingletonProperty(() => new RtslClassifier(types));
        }

        private IClassificationType GetClassificationType(params string[] names)
        {
            foreach (string name in names)
            {
                IClassificationType type = ClassificationRegistry.GetClassificationType(name);
                if (type != null)
                {
                    return type;
                }
            }

            return ClassificationRegistry.GetClassificationType("text");
        }
    }

    internal sealed class RtslClassificationTypes
    {
        public RtslClassificationTypes(
            IClassificationType keyword,
            IClassificationType controlFlow,
            IClassificationType resourceAccess,
            IClassificationType typeName,
            IClassificationType varyingQualifier,
            IClassificationType comment,
            IClassificationType stringLiteral,
            IClassificationType number)
        {
            Keyword = keyword;
            ControlFlow = controlFlow;
            ResourceAccess = resourceAccess;
            TypeName = typeName;
            VaryingQualifier = varyingQualifier;
            Comment = comment;
            StringLiteral = stringLiteral;
            Number = number;
        }

        public IClassificationType Keyword { get; }
        public IClassificationType ControlFlow { get; }
        public IClassificationType ResourceAccess { get; }
        public IClassificationType TypeName { get; }
        public IClassificationType VaryingQualifier { get; }
        public IClassificationType Comment { get; }
        public IClassificationType StringLiteral { get; }
        public IClassificationType Number { get; }
    }

    internal sealed class RtslClassifier : IClassifier
    {
        private static readonly HashSet<string> Keywords = new HashSet<string>(StringComparer.Ordinal)
        {
            "uniform",
            "varying",
            "struct",
            "namespace",
            "using",
            "entry",
            "fn",
            "const",
            "auto",
            "patch",
            "mesh",
            "ray",
            "hit",
            "trace",
        };

        private static readonly HashSet<string> ControlFlow = new HashSet<string>(StringComparer.Ordinal)
        {
            "return",
            "if",
            "else",
            "while",
            "do",
            "for",
        };

        private static readonly HashSet<string> Types = new HashSet<string>(StringComparer.Ordinal)
        {
            "bool",
            "f32",
            "f64",
            "i08",
            "i16",
            "i32",
            "i64",
            "u08",
            "u16",
            "u32",
            "u64",
            "vec2",
            "vec3",
            "vec4",
            "mat2",
            "mat3",
            "mat4",
            "Sampler2D",
            "Triangle",
            "Geometry",
            "Patch",
            "Mesh",
            "Ray",
            "Hit",
            "Task",
            "Payload",
            "void",
        };

        private static readonly HashSet<string> ResourceAccess = new HashSet<string>(StringComparer.Ordinal)
        {
            "readonly",
            "writeonly",
        };

        private static readonly HashSet<string> VaryingQualifiers = new HashSet<string>(StringComparer.Ordinal)
        {
            "clip",
            "smooth",
            "flat",
        };

        private readonly RtslClassificationTypes classificationTypes;

        public RtslClassifier(RtslClassificationTypes classificationTypes)
        {
            this.classificationTypes = classificationTypes;
        }

        public event EventHandler<ClassificationChangedEventArgs> ClassificationChanged;

        public IList<ClassificationSpan> GetClassificationSpans(SnapshotSpan span)
        {
            List<ClassificationSpan> spans = new List<ClassificationSpan>();
            string text = span.GetText();
            int i = 0;
            bool inBlockComment = IsInsideBlockCommentAt(span.Snapshot, span.Start.Position);

            while (i < text.Length)
            {
                char c = text[i];

                if (inBlockComment)
                {
                    int end = text.IndexOf("*/", i, StringComparison.Ordinal);
                    if (end < 0)
                    {
                        AddSpan(spans, span, i, text.Length - i, classificationTypes.Comment);
                        break;
                    }

                    end += 2;
                    AddSpan(spans, span, i, end - i, classificationTypes.Comment);
                    i = end;
                    inBlockComment = false;
                    continue;
                }

                if (c == '/' && i + 1 < text.Length)
                {
                    if (text[i + 1] == '/')
                    {
                        AddSpan(spans, span, i, text.Length - i, classificationTypes.Comment);
                        break;
                    }

                    if (text[i + 1] == '*')
                    {
                        int end = SkipBlockComment(text, i + 2);
                        AddSpan(spans, span, i, end - i, classificationTypes.Comment);
                        i = end;
                        continue;
                    }
                }

                if (c == '"' || c == '\'')
                {
                    int end = SkipString(text, i + 1, c);
                    AddSpan(spans, span, i, end - i, classificationTypes.StringLiteral);
                    i = end;
                    continue;
                }

                if (char.IsDigit(c))
                {
                    int end = SkipNumber(text, i);
                    AddSpan(spans, span, i, end - i, classificationTypes.Number);
                    i = end;
                    continue;
                }

                if (!IsIdentifierStart(c))
                {
                    i++;
                    continue;
                }

                int start = i;
                i++;

                while (i < text.Length && IsIdentifierPart(text[i]))
                {
                    i++;
                }

                string token = text.Substring(start, i - start);
                if (Keywords.Contains(token))
                {
                    AddSpan(spans, span, start, token.Length, classificationTypes.Keyword);
                }
                else if (ControlFlow.Contains(token))
                {
                    AddSpan(spans, span, start, token.Length, classificationTypes.ControlFlow);
                }
                else if (ResourceAccess.Contains(token))
                {
                    AddSpan(spans, span, start, token.Length, classificationTypes.ResourceAccess);
                }
                else if (Types.Contains(token) || VaryingQualifiers.Contains(token))
                {
                    IClassificationType type = Types.Contains(token)
                        ? classificationTypes.TypeName
                        : classificationTypes.VaryingQualifier;

                    AddSpan(spans, span, start, token.Length, type);
                }
            }

            return spans;
        }

        private static void AddSpan(
            ICollection<ClassificationSpan> spans,
            SnapshotSpan sourceSpan,
            int start,
            int length,
            IClassificationType type)
        {
            if (length <= 0 || type == null)
            {
                return;
            }

            SnapshotSpan tokenSpan = new SnapshotSpan(sourceSpan.Snapshot, sourceSpan.Start + start, length);
            spans.Add(new ClassificationSpan(tokenSpan, type));
        }

        private static int SkipBlockComment(string text, int start)
        {
            int end = text.IndexOf("*/", start, StringComparison.Ordinal);
            return end < 0 ? text.Length : end + 2;
        }

        private static bool IsInsideBlockCommentAt(ITextSnapshot snapshot, int position)
        {
            string prefix = snapshot.GetText(0, position);
            bool inBlockComment = false;

            for (int i = 0; i < prefix.Length;)
            {
                if (!inBlockComment && i + 1 < prefix.Length && prefix[i] == '/' && prefix[i + 1] == '*')
                {
                    inBlockComment = true;
                    i += 2;
                    continue;
                }

                if (inBlockComment && i + 1 < prefix.Length && prefix[i] == '*' && prefix[i + 1] == '/')
                {
                    inBlockComment = false;
                    i += 2;
                    continue;
                }

                if (!inBlockComment && i + 1 < prefix.Length && prefix[i] == '/' && prefix[i + 1] == '/')
                {
                    int lineBreak = prefix.IndexOfAny(new[] { '\r', '\n' }, i + 2);
                    if (lineBreak < 0)
                    {
                        break;
                    }

                    i = lineBreak + 1;
                    continue;
                }

                if (!inBlockComment && (prefix[i] == '"' || prefix[i] == '\''))
                {
                    i = SkipString(prefix, i + 1, prefix[i]);
                    continue;
                }

                i++;
            }

            return inBlockComment;
        }

        private static int SkipString(string text, int start, char delimiter)
        {
            for (int i = start; i < text.Length; i++)
            {
                if (text[i] == '\\')
                {
                    i++;
                    continue;
                }

                if (text[i] == delimiter)
                {
                    return i + 1;
                }
            }

            return text.Length;
        }

        private static int SkipNumber(string text, int start)
        {
            int i = start;

            while (i < text.Length && char.IsDigit(text[i]))
            {
                i++;
            }

            if (i < text.Length && text[i] == '.')
            {
                i++;

                while (i < text.Length && char.IsDigit(text[i]))
                {
                    i++;
                }
            }

            if (i < text.Length && (text[i] == 'e' || text[i] == 'E'))
            {
                int exponentStart = i;
                i++;

                if (i < text.Length && (text[i] == '+' || text[i] == '-'))
                {
                    i++;
                }

                if (i < text.Length && char.IsDigit(text[i]))
                {
                    while (i < text.Length && char.IsDigit(text[i]))
                    {
                        i++;
                    }
                }
                else
                {
                    i = exponentStart;
                }
            }

            return i;
        }

        private static bool IsIdentifierStart(char c)
        {
            return char.IsLetter(c) || c == '_';
        }

        private static bool IsIdentifierPart(char c)
        {
            return char.IsLetterOrDigit(c) || c == '_';
        }
    }
}
