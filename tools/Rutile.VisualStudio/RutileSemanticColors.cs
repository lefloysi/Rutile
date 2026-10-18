using System;
using System.Threading.Tasks;
using Microsoft.VisualStudio.LanguageServer.Client;
using Newtonsoft.Json.Linq;

namespace Rutile.VisualStudio {
	internal interface ISemanticColorSink {
		void DocumentChanged(string method, JToken parameters);
	}

	internal sealed class RutileSemanticColors : ILanguageClientMiddleLayer {
		private readonly ISemanticColorSink sink;

		internal RutileSemanticColors(ISemanticColorSink sink) {
			this.sink = sink;
		}

		public bool CanHandle(string methodName) {
			return methodName == "textDocument/didOpen" || methodName == "textDocument/didChange" || methodName == "textDocument/didSave" || methodName == "textDocument/didClose";
		}

		public Task<JToken?> HandleRequestAsync(string methodName, JToken methodParam, Func<JToken, Task<JToken?>> sendRequest) {
			return sendRequest(methodParam);
		}

		public async Task HandleNotificationAsync(string methodName, JToken methodParam, Func<JToken, Task> sendNotification) {
			await sendNotification(methodParam).ConfigureAwait(false);
			sink.DocumentChanged(methodName, methodParam);
		}
	}
}
