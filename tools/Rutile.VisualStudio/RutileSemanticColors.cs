using System;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.VisualStudio.LanguageServer.Client;
using Newtonsoft.Json.Linq;

namespace Rutile.VisualStudio {
	internal interface ISemanticColorSink {
		void Configure(string[] legend);
		void DocumentChanged(string method, JToken parameters);
	}

	internal sealed class RutileSemanticColors : ILanguageClientMiddleLayer {
		private readonly ISemanticColorSink sink;

		internal RutileSemanticColors(ISemanticColorSink sink) {
			this.sink = sink;
		}

		public bool CanHandle(string methodName) {
			return methodName == "initialize" || methodName == "textDocument/didOpen" || methodName == "textDocument/didChange" || methodName == "textDocument/didSave" || methodName == "textDocument/didClose";
		}

		public async Task<JToken?> HandleRequestAsync(string methodName, JToken methodParam, Func<JToken, Task<JToken?>> sendRequest) {
			var response = await sendRequest(methodParam).ConfigureAwait(false);
			if (methodName != "initialize") {
				return response;
			}
			var capabilities = response?["capabilities"] as JObject;
			var names = capabilities?["semanticTokensProvider"]?["legend"]?["tokenTypes"] as JArray;
			sink.Configure(names?.Values<string>().Select(value => value ?? string.Empty).ToArray() ?? Array.Empty<string>());
			// Rutile's classifier consumes the compiler's tokens directly. Leaving the
			// native tagger enabled would add competing, differently styled spans.
			capabilities?.Remove("semanticTokensProvider");
			return response;
		}

		public async Task HandleNotificationAsync(string methodName, JToken methodParam, Func<JToken, Task> sendNotification) {
			await sendNotification(methodParam).ConfigureAwait(false);
			sink.DocumentChanged(methodName, methodParam);
		}
	}
}
