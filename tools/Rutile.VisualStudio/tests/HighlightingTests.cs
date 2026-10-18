using System;
using System.Linq;
using System.Threading.Tasks;
using Newtonsoft.Json.Linq;
using Rutile.VisualStudio;

internal static class HighlightingTests {
	private static void Require(bool value, string message) {
		if (!value) {
			throw new Exception(message);
		}
	}

	private static void Lexical() {
		var text = "fn var struct namespace if return for while break continue switch case default else";
		var tokens = RutileLexicalScanner.Scan(text);
		Require(tokens.Count == 14, "keyword count");
		Require(tokens.Take(4).All(value => value.Kind == LexicalKind.Keyword), "ordinary keyword classification");
		Require(tokens.Skip(4).All(value => value.Kind == LexicalKind.ControlKeyword), "control keywords must remain distinct");
		Require(RutileLexicalScanner.Scan("ifx Return SomeType i32 variable").Count == 0, "identifiers and type spellings require semantic bindings");

		text = "// return if\n/* fn \n for */ \"" + "return" + "\" 42.5e-2 'x'";
		tokens = RutileLexicalScanner.Scan(text);
		Require(tokens.Count == 5, "comment/string/number token count");
		Require(tokens[0].Kind == LexicalKind.Comment && tokens[1].Kind == LexicalKind.Comment, "comments");
		Require(tokens[2].Kind == LexicalKind.String && tokens[3].Kind == LexicalKind.Number && tokens[4].Kind == LexicalKind.String, "literals");
		Require(text.Substring(tokens[3].Start, tokens[3].Length) == "42.5e-2", "exponent span");
		Require(RutileLexicalScanner.Scan("/* if return")[0].Length == 12, "unterminated comment spans to end");
		text = "\"emoji \U0001F600\" return";
		tokens = RutileLexicalScanner.Scan(text);
		Require(text.Substring(tokens[1].Start, tokens[1].Length) == "return", "UTF16 editor offsets");
		text = "\"escaped \\\" if\" return";
		tokens = RutileLexicalScanner.Scan(text);
		Require(tokens.Count == 2 && tokens[0].Kind == LexicalKind.String && tokens[1].Kind == LexicalKind.ControlKeyword, "escaped quotes isolate keywords");
		Console.WriteLine("lexical classes, trivia recovery and UTF16 spans: PASS");
	}

	private static async Task Semantic() {
		var layer = new RutileSemanticColors();
		Require(layer.CanHandle("initialize") && !layer.CanHandle("textDocument/hover"), "initialize only");
		var response = JObject.Parse("{ 'capabilities': { 'semanticTokensProvider': { 'legend': { 'tokenTypes': ['type','class','struct','enum','interface','typeParameter','keyword','number','custom','function','method','namespace','enumMember'], 'tokenModifiers':['declaration','readonly'] }, 'full': true } } }");
		var parameters = new JObject();
		var calls = 0;
		var result = await layer.HandleRequestAsync("initialize", parameters, value => {
			Require(ReferenceEquals(value, parameters), "request is unchanged");
			++calls;
			return Task.FromResult<JToken?>(response);
		});
		Require(calls == 1 && ReferenceEquals(result, response), "response forwarding");
		var legend = result!["capabilities"]!["semanticTokensProvider"]!["legend"]!;
		Require(legend["tokenTypes"]!.Count() == 13, "legend index count must not change");
		Require(legend["tokenTypes"]!.Take(5).All(value => (string?)value == "cppType"), "bound types use C++ type style");
		Require((string?)legend["tokenTypes"]![5] == "cppGenericType", "template parameter classification");
		Require((string?)legend["tokenTypes"]![6] == "keyword" && (string?)legend["tokenTypes"]![8] == "custom", "unmapped tokens unchanged");
		Require((string?)legend["tokenTypes"]![9] == "cppFunction" && (string?)legend["tokenTypes"]![10] == "cppMemberFunction", "functions use C++ function colors");
		Require((string?)legend["tokenTypes"]![11] == "cppNamespace", "namespaces use C++ namespace colors");
		Require((string?)legend["tokenTypes"]![12] == (string?)legend["tokenTypes"]![9], "enum values share the function palette");
		Require(legend["tokenModifiers"]!.Count() == 2 && (string?)legend["tokenModifiers"]![1] == "readonly", "modifiers unchanged");
		var missing = new JObject();
		Require(ReferenceEquals(await layer.HandleRequestAsync("initialize", parameters, _ => Task.FromResult<JToken?>(missing)), missing), "server without semantic tokens");
		Require(await layer.HandleRequestAsync("initialize", parameters, _ => Task.FromResult<JToken?>(null)) == null, "null response");
		Console.WriteLine("semantic legend mapping preserves token indices and protocol: PASS");
	}

	private static int Main() {
		try {
			Lexical();
			Semantic().GetAwaiter().GetResult();
			return 0;
		}
		catch (Exception error) {
			Console.Error.WriteLine(error);
			return 1;
		}
	}
}
