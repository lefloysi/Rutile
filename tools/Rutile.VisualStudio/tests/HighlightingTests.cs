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
		var booleanLiterals = RutileLexicalScanner.Scan("true false");
		Require(booleanLiterals.Count == 2 && booleanLiterals.All(value => value.Kind == LexicalKind.Keyword), "boolean literals are immediately blue");

		text = "// return if\n/* fn \n for */ \"" + "return" + "\" 42.5e-2 'x'";
		tokens = RutileLexicalScanner.Scan(text);
		Require(tokens.Count == 5, "comment/string/number token count");
		Require(tokens[0].Kind == LexicalKind.Comment && tokens[1].Kind == LexicalKind.Comment, "comments");
		Require(tokens[2].Kind == LexicalKind.String && tokens[3].Kind == LexicalKind.Number && tokens[4].Kind == LexicalKind.String, "literals");
		Require(text.Substring(tokens[3].Start, tokens[3].Length) == "42.5e-2", "exponent span");
		Require(RutileLexicalScanner.Scan("/* if return")[0].Length == 12, "unterminated comment spans to end");
		Require(RutileLexicalScanner.MasksSemantic(RutileLexicalScanner.Scan("/* function()"), 3, 8), "new comment masks cached binding");
		Require(RutileLexicalScanner.MasksSemantic(RutileLexicalScanner.Scan("\"function()"), 1, 8), "new string masks cached binding");
		Require(!RutileLexicalScanner.MasksSemantic(RutileLexicalScanner.Scan("/* x */ function()"), 8, 8), "binding after comment remains colored");
		text = "\"emoji \U0001F600\" return";
		tokens = RutileLexicalScanner.Scan(text);
		Require(text.Substring(tokens[1].Start, tokens[1].Length) == "return", "UTF16 editor offsets");
		text = "\"escaped \\\" if\" return";
		tokens = RutileLexicalScanner.Scan(text);
		Require(tokens.Count == 2 && tokens[0].Kind == LexicalKind.String && tokens[1].Kind == LexicalKind.ControlKeyword, "escaped quotes isolate keywords");
		Console.WriteLine("lexical classes, trivia recovery and UTF16 spans: PASS");
	}

	private sealed class Sink : ISemanticColorSink {
		internal string[] Legend = Array.Empty<string>();
		internal int Notifications;

		public void Configure(string[] legend) {
			Legend = legend;
		}

		public void DocumentChanged(string method, JToken parameters) {
			++Notifications;
		}
	}

	private static async Task Semantic() {
		var sink = new Sink();
		var layer = new RutileSemanticColors(sink);
		Require(layer.CanHandle("initialize") && layer.CanHandle("textDocument/didChange") && !layer.CanHandle("textDocument/hover"), "color lifecycle interception only");
		var response = JObject.Parse("{ 'capabilities': { 'hoverProvider':true, 'semanticTokensProvider': { 'legend': { 'tokenTypes': ['namespace','type','struct','enum','function','method','enumMember','keyword','variable'], 'tokenModifiers':['declaration','readonly'] }, 'full': true } } }");
		var parameters = new JObject();
		var calls = 0;
		var result = await layer.HandleRequestAsync("initialize", parameters, value => {
			Require(ReferenceEquals(value, parameters), "request is unchanged");
			++calls;
			return Task.FromResult<JToken?>(response);
		});
		Require(calls == 1 && ReferenceEquals(result, response), "response forwarding");
		Require(sink.Legend.Length == 9 && sink.Legend[0] == "namespace" && sink.Legend[6] == "enumMember", "compiler legend is preserved");
		Require(result!["capabilities"]!["semanticTokensProvider"] == null && (bool)result["capabilities"]!["hoverProvider"]!, "only competing native semantic tagger is disabled");
		await layer.HandleNotificationAsync("textDocument/didChange", parameters, _ => {
			Require(sink.Notifications == 0, "notify server before requesting new colors");
			return Task.CompletedTask;
		});
		Require(sink.Notifications == 1, "color refresh follows document synchronization");

		var text = "sample Item executable value read u08 variable";
		var data = new JArray(0,0,6,0,0, 0,7,4,1,0, 0,5,10,6,0, 0,11,5,4,0, 0,6,4,5,0, 0,5,3,7,0, 0,4,8,8,0);
		var decoded = RutileSemanticDecoder.Decode(text, data, sink.Legend);
		Require(decoded.Count == 6, "only explicitly styled compiler bindings");
		Require(decoded[0].Classification == RutileClassification.Namespace && decoded[1].Classification == RutileClassification.Type, "namespace and type are distinct");
		Require(decoded.Skip(2).Take(3).All(value => value.Classification == RutileClassification.Function), "functions methods enum values share yellow style");
		Require(decoded[5].Classification == RutileClassification.Keyword, "primitive compiler token is blue");
		Require(text.Substring(decoded[3].Start, decoded[3].Length) == "value", "semantic offsets");
		Require(RutileSemanticDecoder.Decode("a", new JArray(0,0,99,0,0), sink.Legend).Count == 0, "invalid spans rejected");
		Require(RutileSemanticDecoder.Decode("a", new JArray(0,0), sink.Legend).Count == 0, "malformed tuple rejected");
		var unicode = "\U0001F600\r\nItem";
		Require(RutileSemanticDecoder.Decode(unicode, new JArray(1,0,4,1,0), sink.Legend)[0].Start == 4, "CRLF UTF16 offsets");
		var edit = JArray.Parse("[{ 'range': {'start':{'line':0,'character':0},'end':{'line':0,'character':2}}, 'text':'x' }]");
		Require(RutileDocumentText.Apply(unicode, edit) == "x\r\nItem", "document synchronization uses UTF16");
		Require(RutileDocumentText.Apply(unicode, JArray.Parse("[{ 'text':'new' }]")) == "new", "full text synchronization");
		Console.WriteLine("explicit semantic classes, native tagger isolation and document synchronization: PASS");
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
