using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Win32;

namespace Rutile.VisualStudio {
	internal static class RutileProjectEnvironment {
		internal static string FindKiln(string root) {
			var configured = Environment.GetEnvironmentVariable("KILN_EXECUTABLE");
			if (!string.IsNullOrWhiteSpace(configured)) {
				if (!File.Exists(configured)) {
					throw new FileNotFoundException("KILN_EXECUTABLE does not name an existing Kiln executable.", configured);
				}
				return Path.GetFullPath(configured);
			}

			var candidates = new List<string>();
			foreach (var directory in (Environment.GetEnvironmentVariable("PATH") ?? string.Empty).Split(Path.PathSeparator)) {
				if (!string.IsNullOrWhiteSpace(directory)) {
					candidates.Add(Path.Combine(directory.Trim().Trim('"'), "kiln.exe"));
				}
			}
			foreach (var hive in new[] { Registry.CurrentUser, Registry.LocalMachine }) {
				using (var key = hive.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\App Paths\kiln.exe")) {
					if (key?.GetValue(null) is string executable && !string.IsNullOrWhiteSpace(executable)) {
						candidates.Add(executable.Trim('"'));
					}
				}
			}
			candidates.Add(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "Kiln", "bin", "kiln.exe"));
			candidates.Add(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Programs", "Kiln", "bin", "kiln.exe"));
			candidates.Add(Path.Combine(root, "distro", "windows-x64", "kiln.exe"));
			foreach (var candidate in candidates) {
				if (File.Exists(candidate)) {
					return Path.GetFullPath(candidate);
				}
			}
			throw new FileNotFoundException("Kiln was not found. Install Kiln, add its bin directory to PATH, or set KILN_EXECUTABLE.");
		}

		internal static async Task ConfigureAsync(string root, CancellationToken token, Action<string> log) {
			if (!File.Exists(Path.Combine(root, "kiln.rt"))) {
				throw new FileNotFoundException("The Rutile document does not belong to a Kiln project. Open a file beneath a kiln.rt project definition.");
			}
			token.ThrowIfCancellationRequested();
			var executable = FindKiln(root);
			log("Preparing Rutile language support for " + root + ".");
			using (var process = new Process {
				StartInfo = new ProcessStartInfo(executable, "configure") {
					WorkingDirectory = root,
					UseShellExecute = false,
					CreateNoWindow = true,
					RedirectStandardOutput = true,
					RedirectStandardError = true
				}
			}) {
				if (!process.Start()) {
					throw new IOException("Kiln could not prepare Rutile language support.");
				}
				var output = process.StandardOutput.ReadToEndAsync();
				var errors = process.StandardError.ReadToEndAsync();
				using (token.Register(() => {
					try {
						if (!process.HasExited) {
							process.Kill();
						}
					}
					catch (InvalidOperationException) {
					}
					catch (System.ComponentModel.Win32Exception) {
					}
				})) {
					await Task.Run(() => process.WaitForExit()).ConfigureAwait(false);
				}
				token.ThrowIfCancellationRequested();
				var standardOutput = await output.ConfigureAwait(false);
				var standardError = await errors.ConfigureAwait(false);
				if (!string.IsNullOrWhiteSpace(standardOutput)) {
					log(standardOutput.Trim());
				}
				if (!string.IsNullOrWhiteSpace(standardError)) {
					log(standardError.Trim());
				}
				if (process.ExitCode != 0) {
					throw new IOException("Kiln could not configure the Rutile project. " + (string.IsNullOrWhiteSpace(standardError) ? standardOutput : standardError).Trim());
				}
			}
		}
	}
}
