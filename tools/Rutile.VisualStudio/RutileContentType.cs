using System.ComponentModel.Composition;
using Microsoft.VisualStudio.LanguageServer.Client;
using Microsoft.VisualStudio.Utilities;

namespace Rutile.VisualStudio {
	internal static class RutileContentType {
		public const string Name = "Rutile";

		[Export, Name(Name), BaseDefinition("text"), BaseDefinition(CodeRemoteContentDefinition.CodeRemoteBaseTypeName)]
		internal static ContentTypeDefinition Definition = null!;

		[Export, FileExtension(".rt"), ContentType(Name)]
		internal static FileExtensionToContentTypeDefinition SourceExtension = null!;
	}
}
