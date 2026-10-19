using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json.Linq;

namespace Rutile.VisualStudio {
	internal sealed class KilnRequestException : InvalidOperationException {
		public KilnRequestException(string diagnostics) : base(diagnostics) {
		}
	}

	internal sealed class KilnProject {
		private readonly string executable;
		private readonly Action<string> log;
		public string Root { get; }

		public KilnProject(string executable, string root, Action<string>? log = null) {
			this.executable = executable;
			this.log = log ?? ServerOutput.Write;
			Root = root;
		}

		public static string Quote(string value) {
			var result = new StringBuilder("\"");
			var slashes = 0;
			foreach (var character in value) {
				if (character == '\\') {
					slashes++;
					continue;
				}
				result.Append('\\', character == '"' ? slashes * 2 + 1 : slashes);
				result.Append(character);
				slashes = 0;
			}
			result.Append('\\', slashes * 2);
			return result.Append('"').ToString();
		}

		public async Task<JObject> RequestAsync(string operation, string profile, string? target = null) {
			var arguments = operation + " --json --profile " + Quote(profile);
			if (!string.IsNullOrEmpty(target)) {
				arguments += " " + Quote(target!);
			}
			using (var process = new Process {
				StartInfo = new ProcessStartInfo(executable, arguments) {
					WorkingDirectory = Root,
					UseShellExecute = false,
					CreateNoWindow = true,
					RedirectStandardOutput = true,
					RedirectStandardError = true,
					StandardOutputEncoding = Encoding.UTF8,
					StandardErrorEncoding = Encoding.UTF8
				}
			}) {
				process.Start();
				var output = process.StandardOutput.ReadToEndAsync();
				var errors = process.StandardError.ReadToEndAsync();
				await Task.Run(() => process.WaitForExit()).ConfigureAwait(false);
				var text = await output.ConfigureAwait(false);
				var diagnostics = await errors.ConfigureAwait(false);
				if (!string.IsNullOrWhiteSpace(diagnostics)) {
					log(diagnostics);
				}
				JObject response;
				try {
					response = JObject.Parse(text);
				}
				catch (Exception error) when (error is Newtonsoft.Json.JsonException) {
					throw new InvalidOperationException("Kiln returned an invalid response: " + text + diagnostics, error);
				}
				if (process.ExitCode != 0 || response.Value<bool?>("succeeded") == false) {
					throw new KilnRequestException(response.Value<string>("diagnostics") ?? diagnostics ?? "Kiln could not complete the operation.");
				}
				return response;
			}
		}
	}
}
