using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using Rutile.VisualStudio;
using StreamJsonRpc;

public static partial class EditorObservations {
	private static async Task<string> RunAsync(string executable, string directory, string arguments) {
		using (var process = Process.Start(new ProcessStartInfo(executable, arguments) {
			WorkingDirectory = directory, UseShellExecute = false, CreateNoWindow = true,
			RedirectStandardOutput = true, RedirectStandardError = true
		}) ?? throw new IOException("Cannot start Kiln.")) {
			var output = process.StandardOutput.ReadToEndAsync();
			var errors = process.StandardError.ReadToEndAsync();
			if (!await Task.Run(() => process.WaitForExit(60000)).ConfigureAwait(false)) {
				process.Kill();
				throw new TimeoutException("Nested Kiln fixture did not finish.");
			}
			var text = await output.ConfigureAwait(false);
			var error = await errors.ConfigureAwait(false);
			if (process.ExitCode != 0) {
				throw new IOException("Kiln " + arguments + " failed: " + text + error);
			}
			return text;
		}
	}

	private static Task<string> KilnAsync(string directory, string arguments) {
		var executable = Environment.GetEnvironmentVariable("KILN_EXECUTABLE") ?? throw new InvalidOperationException("Kiln did not supply its executable.");
		return RunAsync(executable, directory, arguments);
	}

	private static Task<string> StopCompilerAsync(string directory) {
		return RunAsync(Environment.GetEnvironmentVariable("RUTILE_COMPILER") ?? "rutilec", directory, "exit");
	}

	public static string Service() {
		return ObserveServiceAsync().GetAwaiter().GetResult();
	}

	private static async Task<string> ObserveServiceAsync() {
		var root = Path.Combine(Path.GetTempPath(), "rutile-editor-" + Guid.NewGuid().ToString("N"));
		Directory.CreateDirectory(root);
		var sourcePath = Path.Combine(root, "source.rt");
		var source = "fn answer() -> i32 {\n\treturn 42;\n};\nfn use() -> i32 {\n\treturn answer();\n};\nfn text() -> rt::string {\n\treturn rt::string(\"hello\");\n};\n";
		File.WriteAllText(sourcePath, source, new UTF8Encoding(false));
		File.WriteAllText(Path.Combine(root, "kiln.rt"), "fn build(kiln::Options& options) {\n\tvar target = kiln::add_target(\"fixture\");\n\t@target.sources.add(\"source.rt\");\n\t@target.import(\"std\");\n};\n", new UTF8Encoding(false));
		var stopped = false;
		Exception? failure = null;
		try {
			await KilnAsync(root, "configure").ConfigureAwait(false);
			var build = await KilnAsync(root, "build fixture").ConfigureAwait(false);
			using (var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(30))) {
				var disconnected = 0;
				using (var connection = await RutileServiceConnection.ConnectAsync(root, cancellation.Token, value => {
					Interlocked.Increment(ref disconnected);
					return Task.CompletedTask;
				}, value => { }).ConfigureAwait(false))
				using (var handler = new HeaderDelimitedMessageHandler(connection.Connection.Writer, connection.Connection.Reader))
				using (var rpc = new JsonRpc(handler)) {
					rpc.StartListening();
					var initialized = await rpc.InvokeWithParameterObjectAsync<JObject>("initialize", new JObject {
						["rootUri"] = new Uri(root + Path.DirectorySeparatorChar).AbsoluteUri
					}, cancellation.Token).ConfigureAwait(false);
					var uri = new Uri(sourcePath).AbsoluteUri;
					await rpc.NotifyWithParameterObjectAsync("textDocument/didOpen", new JObject { ["textDocument"] = new JObject { ["uri"] = uri, ["languageId"] = "rutile", ["version"] = 1, ["text"] = source } }).ConfigureAwait(false);
					JObject status;
					JToken? document;
					do {
						status = await rpc.InvokeWithParameterObjectAsync<JObject>("rutile/indexStatus", new JObject(), cancellation.Token).ConfigureAwait(false);
						document = null;
						foreach (var item in status["documents"] ?? new JArray()) {
							if (item.Value<string>("uri") == uri) {
								document = item;
							}
						}
						if (document?.Value<int?>("indexedVersion") != 1) {
							await Task.Delay(100, cancellation.Token).ConfigureAwait(false);
						}
					} while (document?.Value<int?>("indexedVersion") != 1);
					var parameters = new JObject { ["textDocument"] = new JObject { ["uri"] = uri } };
					var tokens = await rpc.InvokeWithParameterObjectAsync<JObject>("textDocument/semanticTokens/full", parameters, cancellation.Token).ConfigureAwait(false);
					parameters["position"] = new JObject { ["line"] = 4, ["character"] = 10 };
					var definitions = await rpc.InvokeWithParameterObjectAsync<JArray>("textDocument/definition", parameters, cancellation.Token).ConfigureAwait(false);
					var legend = initialized["capabilities"]?["semanticTokensProvider"]?["legend"]?["tokenTypes"]?.ToObject<string[]>() ?? Array.Empty<string>();
					var decoded = RutileSemanticDecoder.Decode(source, (JArray)tokens["data"]!, legend);
					var observations = new JArray();
					foreach (var token in decoded) {
						observations.Add(new JObject { ["text"] = source.Substring(token.Start, token.Length), ["classification"] = token.Classification, ["start"] = token.Start });
					}
					connection.Dispose();
					var disposed = connection.IsDisposed;
					var disposalNotifications = disconnected;
					using (var lifetime = new CancellationTokenSource())
					using (var cancelled = await RutileServiceConnection.ConnectAsync(root, lifetime.Token, value => Task.CompletedTask, value => { }).ConfigureAwait(false)) {
						lifetime.Cancel();
						var cancelledDisposed = cancelled.IsDisposed;
						var closed = new TaskCompletionSource<bool>(TaskCreationOptions.RunContinuationsAsynchronously);
						using (var unexpected = await RutileServiceConnection.ConnectAsync(root, CancellationToken.None, value => {
							closed.TrySetResult(true);
							return Task.CompletedTask;
						}, value => { }).ConfigureAwait(false)) {
							await StopCompilerAsync(root).ConfigureAwait(false);
							stopped = true;
							try {
								await unexpected.Connection.Reader.ReadAsync(new byte[1], 0, 1, cancellation.Token).ConfigureAwait(false);
							}
							catch (IOException) {
								// A broken pipe and EOF both notify the production connection owner.
							}
							var notification = await Task.WhenAny(closed.Task, Task.Delay(5000, cancellation.Token)).ConfigureAwait(false);
							return JsonConvert.SerializeObject(new { uri, definitions, colors = observations, disposed, disposal_notifications = disposalNotifications,
								cancelled_disposed = cancelledDisposed, unexpected_disposed = unexpected.IsDisposed,
								unexpected_notification = ReferenceEquals(notification, closed.Task), build });
						}
					}
				}
			}
		}
		catch (Exception error) {
			failure = error;
			throw;
		}
		finally {
			if (!stopped) {
				try {
					await StopCompilerAsync(root).ConfigureAwait(false);
				}
				catch (Exception cleanupError) when (failure != null) {
					Console.Error.WriteLine("Compiler cleanup also failed: " + cleanupError);
				}
			}
		}
	}
}
