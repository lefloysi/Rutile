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
		Directory.CreateDirectory(Path.Combine(root, "support"));
		var source = "[[test]]\nfn label_uses_imported_view() {\n\tLabel label;\n\t@label.value = 1;\n\trt::string_view view = \"view\";\n\trt::test::expect(label.value == 1 && view == \"view\");\n};\n";
		File.WriteAllText(Path.Combine(root, "checks.rt"), source, new UTF8Encoding(false));
		File.WriteAllText(Path.Combine(root, "model", "model.rt"), "export struct Label {\n\ti32 value;\n};\n", new UTF8Encoding(false));
		File.WriteAllText(Path.Combine(root, "model", "native.cpp"), "static_assert(sizeof(void*) == 8);\n", new UTF8Encoding(false));
		File.WriteAllText(Path.Combine(root, "support", "record.cpp"), "struct Record {\n\tint value;\n};\nstatic_assert(sizeof(Record) == sizeof(int));\n", new UTF8Encoding(false));
		File.WriteAllText(Path.Combine(root, "subject", "main.cpp"), "int main() {\n\treturn 0;\n}\n", new UTF8Encoding(false));
		File.WriteAllText(Path.Combine(root, "subject", "subject.rt"), "export fn application_view() -> rt::string_view {\n\treturn rt::string_view(\"view\");\n};\n", new UTF8Encoding(false));
		File.WriteAllText(Path.Combine(root, "unrelated.rt"), "struct Label {\n\ti32 unrelated;\n};\n", new UTF8Encoding(false));
		File.WriteAllText(Path.Combine(root, "kiln.rt"), @"fn build(kiln::Options& options) {
	var model = kiln::add_target(""model"");
	@model.option(""runtime"", ""MT"");
	@model.sources.add(""model"");
	var support = kiln::add_target(""support"");
	@support.option(""runtime"", ""MT"");
	@support.sources.add(""support"");
	var subject = kiln::add_target(""subject"", kiln::TargetType::executable);
	@subject.option(""runtime"", ""MT"");
	@subject.sources.add(""subject"");
	@subject.depends_on(""model"");
	@subject.depends_on(""support"");
	@subject.import(""std"");
	var unrelated = kiln::add_target(""unrelated"");
	@unrelated.sources.add(""unrelated.rt"");
	var checks = kiln::add_tests(subject);
	@checks.sources.add(""checks.rt"");
};
", new UTF8Encoding(false));
		Exception? failure = null;
		try {
			await KilnAsync(root, "build subject").ConfigureAwait(false);
			var compilerPath = Environment.GetEnvironmentVariable("RUTILE_COMPILER") ?? "rutilec";
			var beforeConfigure = JObject.Parse(await RunAsync(compilerPath, root, "request hello").ConfigureAwait(false));
			await KilnAsync(root, "configure --profile debug").ConfigureAwait(false);
			var afterConfigure = JObject.Parse(await RunAsync(compilerPath, root, "request hello").ConfigureAwait(false));
			source += "// Ordinary document edit after configuration.\n";
			File.WriteAllText(Path.Combine(root, "checks.rt"), source, new UTF8Encoding(false));
			await Task.Delay(500).ConfigureAwait(false);
			var afterEdit = JObject.Parse(await RunAsync(compilerPath, root, "request hello").ConfigureAwait(false));
			await KilnAsync(root, "test subject --profile debug").ConfigureAwait(false);
			using (var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(30))) {
				var compiler = Environment.GetEnvironmentVariable("RUTILE_COMPILER") ?? "rutilec";
				JObject status;
				JObject? watch;
				int compiling;
				int nativeWatchSessions;
				int currentTestSessions;
				do {
					status = JObject.Parse(await RunAsync(compiler, root, "request hello").ConfigureAwait(false));
					watch = null;
					compiling = 0;
					nativeWatchSessions = 0;
					currentTestSessions = 0;
					foreach (JObject session in status["sessions"] ?? new JArray()) {
						if (session.Value<string>("target") == "subject" && session.Value<string>("kind") == "test" && session.Value<string>("profile") == "debug" && session.Value<bool>("artifacts_current")) {
							++currentTestSessions;
							watch = session;
						}
						if (session.Value<bool>("compiling")) {
							++compiling;
						}
						if (session.Value<string>("target") == "support") {
							++nativeWatchSessions;
						}
					}
					if (watch == null || compiling > 0) {
						await Task.Delay(100, timeout.Token).ConfigureAwait(false);
					}
				} while (watch == null || compiling > 0);
				var beforeBackground = (JObject)watch.DeepClone();
				var beforeBackgroundCompilations = status.Value<long>("compilations");
				var compiledSession = watch.Value<string>("session");
				var compiledRevision = watch.Value<long>("source_revision");
				source = source.Replace("label.value = 1", "label.value = 2").Replace("label.value == 1", "label.value == 2");
				File.WriteAllText(Path.Combine(root, "checks.rt"), source, new UTF8Encoding(false));
				while (true) {
					status = JObject.Parse(await RunAsync(compiler, root, "request hello").ConfigureAwait(false));
					watch = null;
					foreach (JObject session in status["sessions"] ?? new JArray()) {
						if (session.Value<string>("session") == compiledSession) {
							watch = session;
							break;
						}
					}
					if (
						watch != null
						&& watch.Value<long>("source_revision") > compiledRevision
						&& watch.Value<long>("artifact_revision") == watch.Value<long>("source_revision")
						&& watch.Value<bool>("artifacts_current")
						&& !watch.Value<bool>("compiling")
					) {
						break;
					}
					await Task.Delay(100, timeout.Token).ConfigureAwait(false);
				}
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
					return new JObject { ["watch"] = watch, ["native_watch_sessions"] = nativeWatchSessions,
						["before_background"] = beforeBackground,
						["before_background_compilations"] = beforeBackgroundCompilations,
						["after_background_compilations"] = status.Value<long>("compilations"),
						["current_test_sessions"] = currentTestSessions,
						["before_configure"] = beforeConfigure, ["after_configure"] = afterConfigure, ["after_edit"] = afterEdit,
						["progress"] = progress, ["diagnostics"] = diagnostics.For(uri), ["colors"] = colors }.ToString();
				}
			}
		}
		catch (Exception error) {
			failure = error;
			throw;
		}
		finally {
			try {
				await StopCompilerAsync(root).ConfigureAwait(false);
			}
			catch (Exception cleanupError) when (failure != null) {
				Console.Error.WriteLine("Compiler cleanup also failed: " + cleanupError);
			}
		}
	}
}
