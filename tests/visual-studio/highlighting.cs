using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using Rutile.VisualStudio;

public static partial class EditorObservations {
	public static string Lexical() {
		var text = "// return if\n/* fn \n for */ \"return\" 42.5e-2 'x'";
		var tokens = RutileLexicalScanner.Scan(text).Select(token => new {
			kind = token.Kind.ToString(),
			start = token.Start,
			length = token.Length,
			text = text.Substring(token.Start, token.Length)
		}).ToArray();
		var unicode = RutileLexicalScanner.Scan("\"emoji \U0001F600\" return");
		return JsonConvert.SerializeObject(new {
			identifier_tokens = RutileLexicalScanner.Scan("fn var namespace return i32 SomeType variable").Count,
			tokens,
			unicode_length = unicode[0].Length,
			unterminated_length = RutileLexicalScanner.Scan("/* if return")[0].Length,
			escaped_tokens = RutileLexicalScanner.Scan("\"escaped \\\" if\" return").Count
		});
	}

	public static string Decoder() {
		var reordered = RutileSemanticDecoder.Decode("future", new JArray(0, 0, 6, 1, 0), new[] { "function", "controlKeyword" });
		var unicode = "\U0001F600\r\nItem";
		var decoded = RutileSemanticDecoder.Decode(unicode, new JArray(1, 0, 4, 0, 0), new[] { "type" });
		var edit = JArray.Parse("[{ 'range': { 'start': { 'line': 0, 'character': 0 }, 'end': { 'line': 0, 'character': 2 } }, 'text': 'x' }]");
		var roles = new[] { "controlKeyword", "comment", "string", "number", "operator", "keyword" };
		var roleTokens = new JArray(0, 0, 1, 0, 0, 0, 2, 1, 1, 0, 0, 2, 1, 2, 0, 0, 2, 1, 3, 0, 0, 2, 1, 4, 0, 0, 2, 1, 5, 0);
		var bindings = RutileSemanticDecoder.Decode("ns Item enumval fn method u08 variable", new JArray(0, 0, 2, 0, 0, 0, 3, 4, 1, 0, 0, 5, 7, 2, 0, 0, 8, 2, 3, 0, 0, 3, 6, 4, 0, 0, 7, 3, 5, 0, 0, 4, 8, 6, 0), new[] { "namespace", "type", "enumMember", "function", "method", "keyword", "variable" });
		return JsonConvert.SerializeObject(new {
			roles = RutileSemanticDecoder.Decode("a b c d e f", roleTokens, roles).Select(span => span.Classification).ToArray(),
			bindings = bindings.Select(span => span.Classification).ToArray(),
			classification = reordered[0].Classification,
			unicode_start = decoded[0].Start,
			unicode_length = decoded[0].Length,
			modified = RutileDocumentText.Apply(unicode, edit),
			replaced = RutileDocumentText.Apply(unicode, JArray.Parse("[{ 'text': 'new' }]")),
			malformed_count = RutileSemanticDecoder.Decode("item", new JArray(0, 0, 4), new[] { "type" }).Count,
			out_of_range_count = RutileSemanticDecoder.Decode("item", new JArray(0, 4, 5, 0, 0), new[] { "type" }).Count
		});
	}

	private sealed class ColorSink : ISemanticColorSink {
		internal readonly List<string> Events = new List<string>();
		public void DocumentChanged(string method, JToken parameters) {
			Events.Add(method);
		}
	}

	public static string MiddleLayer() {
		var sink = new ColorSink();
		var layer = new RutileSemanticColors(sink);
		var parameters = JObject.Parse("{ 'textDocument': { 'uri': 'file:///source.rt', 'text': 'fn answer();' } }");
		layer.HandleNotificationAsync("textDocument/didOpen", parameters, value => {
			sink.Events.Add("forward");
			return Task.CompletedTask;
		}).GetAwaiter().GetResult();
		var forwarded = layer.HandleRequestAsync("textDocument/hover", parameters, value => Task.FromResult<JToken?>(new JObject { ["answer"] = 42 })).GetAwaiter().GetResult();
		return JsonConvert.SerializeObject(new {
			open_handled = layer.CanHandle("textDocument/didOpen"),
			change_handled = layer.CanHandle("textDocument/didChange"),
			initialize_handled = layer.CanHandle("initialize"),
			hover_handled = layer.CanHandle("textDocument/hover"),
			events = sink.Events,
			forwarded = forwarded?["answer"]
		});
	}

	public static string BraceEligibility() {
		var accepted = new[] { "", "fn f", "/* closed */", "// comment\n", "\"closed\"", "'x'" };
		var rejected = new[] { "// comment", "/* comment", "\"literal", "'literal", "\"escaped\\\"" };
		return JsonConvert.SerializeObject(new {
			accepted = accepted.Select(prefix => RutileBraceCompletionProvider.CanStart(prefix, '(', ')')).ToArray(),
			rejected = rejected.Select(prefix => RutileBraceCompletionProvider.CanStart(prefix, '(', ')')).ToArray(),
			angle_brackets = RutileBraceCompletionProvider.CanStart("template", '<', '>'),
			pairs = typeof(RutileBraceCompletionProvider).GetCustomAttributesData().Where(attribute => attribute.AttributeType.Name == "BracePairAttribute").Select(attribute => "" + (char)attribute.ConstructorArguments[0].Value! + (char)attribute.ConstructorArguments[1].Value!).OrderBy(value => value, StringComparer.Ordinal).ToArray(),
			default_pairs = typeof(RutileBraceCompletionDefaults).GetCustomAttributesData().Where(attribute => attribute.AttributeType.Name == "BracePairAttribute").Select(attribute => "" + (char)attribute.ConstructorArguments[0].Value! + (char)attribute.ConstructorArguments[1].Value!).OrderBy(value => value, StringComparer.Ordinal).ToArray(),
			tab_indent = RutileBraceCompletionContext.BodyIndent("\t", 4, 4, false),
			space_indent = RutileBraceCompletionContext.BodyIndent("  ", 2, 4, true),
			partial_tab_indent = RutileBraceCompletionContext.BodyIndent("", 4, 8, false),
			escaped = RutileBraceCompletionProvider.Escaped("\\\\\\\"", 3)
		});
	}
}
