using System.ComponentModel.Composition;
using Microsoft.VisualStudio.Utilities;

namespace vs_rtsl_ext
{
    internal static class RtslContentType
    {
        public const string Name = "rtsl";

        [Export]
        [Name(Name)]
        [BaseDefinition("code")]
        internal static ContentTypeDefinition ContentTypeDefinition;

        [Export]
        [FileExtension(".rtsl")]
        [ContentType(Name)]
        internal static FileExtensionToContentTypeDefinition FileExtensionDefinition;
    }
}
