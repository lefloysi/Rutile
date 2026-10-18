using System.IO;

namespace Microsoft.VisualStudio.LanguageServer.Client {
	// The real SDK is checked by the extension build. These streams isolate process ownership tests.
	internal sealed class Connection {
		public Stream Input { get; }
		public Stream Output { get; }

		public Connection(Stream input, Stream output) {
			Input = input;
			Output = output;
		}
	}
}
