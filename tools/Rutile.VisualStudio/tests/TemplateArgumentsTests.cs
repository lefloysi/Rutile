using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
using Newtonsoft.Json.Linq;
using Rutile.VisualStudio;

internal class TemplateArgumentsTests {
	[STAThread]
	public static int Main() {
		if (TemplateContextResponse.Declaration(new JArray(), 4, 4) != null) {
			throw new Exception("Pending response accepted");
		}
		var response = JObject.Parse("{\"template\":{\"name\":\"Box\"}}");
		if (TemplateContextResponse.Declaration(response, 4, 5) != null) {
			throw new Exception("Stale snapshot accepted");
		}
		if ((string)TemplateContextResponse.Declaration(response, 4, 4)!["name"]! != "Box") {
			throw new Exception("Current declaration lost");
		}
		Console.WriteLine("Template arguments pending response and snapshot checks: PASS");
		return 0;
	}
}
