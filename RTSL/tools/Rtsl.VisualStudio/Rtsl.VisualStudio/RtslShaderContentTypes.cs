using Microsoft.VisualStudio.LanguageServer.Client;
using Microsoft.VisualStudio.Utilities;
using System.ComponentModel.Composition;

namespace Rtsl.VisualStudio
{
    internal static class RtslShaderContentTypes
    {
        public const string RtslShader = "rtslShader";

#pragma warning disable 649
        [Export]
        [Name(RtslShader)]
        [BaseDefinition(CodeRemoteContentDefinition.CodeRemoteContentTypeName)]
        [BaseDefinition(StandardContentTypeNames.Code)]
        internal static readonly ContentTypeDefinition RtslShaderContentType;

        [Export]
        [FileExtension(".rtsl")]
        [ContentType(RtslShader)]
        internal static readonly FileExtensionToContentTypeDefinition RtslShaderFileExtension;
#pragma warning restore 649
    }
}
