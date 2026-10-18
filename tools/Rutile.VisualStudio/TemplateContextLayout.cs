using System;
using Newtonsoft.Json.Linq;
using System.Windows;
using System.Windows.Controls;

namespace Rutile.VisualStudio {
	internal static class TemplateContextResponse {
		internal static JObject? Declaration(JToken? response, int requestedVersion, int currentVersion) {
			if (requestedVersion != currentVersion || !(response is JObject result)) {
				return null;
			}
			return result["template"] as JObject;
		}
	}
}
