using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Newtonsoft.Json.Linq;
using Rutile.VisualStudio;
using StreamJsonRpc;

internal sealed class EditorDiagnostics {
	private readonly object gate = new object();
	private readonly Dictionary<string, JArray> documents = new Dictionary<string, JArray>();

	[JsonRpcMethod("textDocument/publishDiagnostics", UseSingleObjectParameterDeserialization = true)]
	public void Publish(JObject parameters) {
		lock (gate) {
			documents[parameters.Value<string>("uri")!] = (JArray)parameters["diagnostics"]!;
		}
	}

	internal JArray? For(string uri) {
		lock (gate) {
			return documents.TryGetValue(uri, out var diagnostics) ? (JArray)diagnostics.DeepClone() : null;
		}
	}
}

public static partial class EditorObservations {
	public static string WatchDependencies() {
		return ObserveWatchDependenciesAsync().GetAwaiter().GetResult();
	}

	private static async Task<string> ObserveWatchDependenciesAsync() {
		var root = Path.Combine(Path.GetTempPath(), "rutile-editor-watch-" + Guid.NewGuid().ToString("N"));
		Directory.CreateDirectory(Path.Combine(root, "model"));
		Directory.CreateDirectory(Path.Combine(root, "subject"));
		var source = "[[test]]\nfn label_uses_imported_view() {\n\tLabel label;\n\t@label.value = 1;\n\trt::string_view view = \"view\";\n\trt::test::expect(label.value == 1 && view == \"view\");\n};\n";
		File.WriteAllText(Path.Combine(root, "checks.rt"), source, new UTF8Encoding(false));
		File.WriteAllText(Path.Combine(root, "model", "model.rt"), "export struct Label {\n\ti32 value;\n};\n", new UTF8Encoding(false));
		File.WriteAllText(Path.Combine(root, "model", "native.cpp"), "static_assert(sizeof(void*) == 8);\n", new UTF8Encoding(false));
		File.WriteAllText(Path.Combine(root, "subject", "main.cpp"), "int main() {\n\treturn 0;\n}\n", new UTF8Encoding(false));
		File.WriteAllText(Path.Combine(root, "subject", "subject.rt"), "export fn application_view() -> rt::string_view {\n\treturn rt::string_view(\"view\");\n};\n", new UTF8Encoding(false));
		File.WriteAllText(Path.Combine(root, "unrelated.rt"), "struct Label {\n\ti32 unrelated;\n};\n", new UTF8Encoding(false));
		File.WriteAllText(Path.Combine(root, "kiln.rt"), @"fn build(kiln::Options& options) {
	var model = kiln::add_target(""model"");
	@model.compiler(""cpp"");
	@model.option(""runtime"", ""MT"");
	@model.sources.add(""model"");
	var subject = kiln::add_target(""subject"", kiln::TargetType::executable);
	@subject.compiler(""cpp"");
	@subject.option(""runtime"", ""MT"");
	@subject.sources.add(""subject"");
	@subject.depends_on(""model"");
	@subject.import(""std"");
	var unrelated = kiln::add_target(""unrelated"");
	@unrelated.sources.add(""unrelated.rt"");
	var checks = kiln::add_tests(""checks"", subject);
	@checks.sources.add(""checks.rt"");
};
", new UTF8Encoding(false));
		try {
			await KilnAsync(root, "build subject").ConfigureAwait(false);
			await KilnAsync(root, "configure --profile debug").ConfigureAwait(false);
			using (var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(30))) {
				var compiler = Environment.GetEnvironmentVariable("RUTILE_COMPILER") ?? "rutilec";
				JObject status;
				JObject? watch;
				int compiling;
				do {
					status = JObject.Parse(await RunAsync(compiler, root, "request hello").ConfigureAwait(false));
					watch = null;
					compiling = 0;
					foreach (JObject session in status["sessions"] ?? new JArray()) {
						if (session.Value<string>("target") == "checks" && session.Value<string>("profile") == "debug") {
							watch = session;
						}
						if (session.Value<bool>("compiling")) {
							++compiling;
						}
					}
					if (watch == null || compiling > 0) {
						await Task.Delay(100, timeout.Token).ConfigureAwait(false);
					}
				} while (watch == null || compiling > 0);
				var progress = JObject.Parse(await RunAsync(compiler, root, "request progress").ConfigureAwait(false));
				using (var connection = await RutileServiceConnection.ConnectAsync(root, timeout.Token, value => Task.CompletedTask, value => { }).ConfigureAwait(false))
				using (var handler = new HeaderDelimitedMessageHandler(connection.Connection.Writer, connection.Connection.Reader))
				using (var rpc = new JsonRpc(handler)) {
					var diagnostics = new EditorDiagnostics();
					rpc.AddLocalRpcTarget(diagnostics);
					rpc.StartListening();
					var initialized = await rpc.InvokeWithParameterObjectAsync<JObject>("initialize", new JObject {
						["rootUri"] = new Uri(root + Path.DirectorySeparatorChar).AbsoluteUri
					}, timeout.Token).ConfigureAwait(false);
					var uri = new Uri(Path.Combine(root, "checks.rt")).AbsoluteUri;
					await rpc.NotifyWithParameterObjectAsync("textDocument/didOpen", new JObject { ["textDocument"] = new JObject {
						["uri"] = uri, ["languageId"] = "rutile", ["version"] = 1, ["text"] = source
					} }).ConfigureAwait(false);
					JToken? document;
					do {
						var indexing = await rpc.InvokeWithParameterObjectAsync<JObject>("rutile/indexStatus", new JObject(), timeout.Token).ConfigureAwait(false);
						document = null;
						foreach (var item in indexing["documents"] ?? new JArray()) {
							if (item.Value<string>("uri") == uri) {
								document = item;
							}
						}
						if (document?.Value<int?>("indexedVersion") != 1 || diagnostics.For(uri) == null) {
							await Task.Delay(100, timeout.Token).ConfigureAwait(false);
						}
					} while (document?.Value<int?>("indexedVersion") != 1 || diagnostics.For(uri) == null);
					var tokens = await rpc.InvokeWithParameterObjectAsync<JObject>("textDocument/semanticTokens/full", new JObject {
						["textDocument"] = new JObject { ["uri"] = uri }
					}, timeout.Token).ConfigureAwait(false);
					var legend = initialized["capabilities"]!["semanticTokensProvider"]!["legend"]!["tokenTypes"]!.ToObject<string[]>()!;
					var colors = new JArray();
					foreach (var token in RutileSemanticDecoder.Decode(source, (JArray)tokens["data"]!, legend)) {
						colors.Add(new JObject { ["text"] = source.Substring(token.Start, token.Length), ["classification"] = token.Classification });
					}
					return new JObject { ["watch"] = watch, ["progress"] = progress, ["diagnostics"] = diagnostics.For(uri), ["colors"] = colors }.ToString();
				}
			}
		}
		finally {
			await StopCompilerAsync(root).ConfigureAwait(false);
		}
	}
}
