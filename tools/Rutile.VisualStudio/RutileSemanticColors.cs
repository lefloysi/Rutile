using System;
using System.Threading.Tasks;
using Microsoft.VisualStudio.LanguageServer.Client;
using Newtonsoft.Json.Linq;

namespace Rutile.VisualStudio {
	internal sealed class RutileSemanticColors : ILanguageClientMiddleLayer {
		public bool CanHandle(string methodName) {
			return methodName == "initialize";
		}

		public async Task<JToken?> HandleRequestAsync(string methodName, JToken methodParam, Func<JToken, Task<JToken?>> sendRequest) {
			var response = await sendRequest(methodParam).ConfigureAwait(false);
			if (methodName != "initialize") {
				return response;
			}
			var names = response?["capabilities"]?["semanticTokensProvider"]?["legend"]?["tokenTypes"] as JArray;
			if (names == null) {
				return response;
			}
			for (var index = 0; index < names.Count; ++index) {
				var name = names[index].Type == JTokenType.String ? (string?)names[index] : null;
				var mapped = Classification(name);
				if (mapped != null) {
					names[index] = mapped;
				}
			}
			return response;
		}

		public Task HandleNotificationAsync(string methodName, JToken methodParam, Func<JToken, Task> sendNotification) {
			return sendNotification(methodParam);
		}

		private static string? Classification(string? token) {
			switch (token) {
				case "type":
				case "class":
				case "struct":
				case "enum":
				case "interface":
					return "cppType";
				case "typeParameter":
					return "cppGenericType";
				case "namespace":
					return "cppNamespace";
				case "function":
					return "cppFunction";
				case "method":
					return "cppMemberFunction";
				case "parameter":
					return "cppParameter";
				case "property":
					return "cppMemberField";
				case "enumMember":
					return "cppFunction";
				default:
					return null;
			}
		}
	}
}
