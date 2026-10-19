using System;
using System.IO;
using System.Text.RegularExpressions;
using Microsoft.VisualStudio.Shell;

namespace Rutile.VisualStudio {
	internal sealed class KilnDiagnostics : IDisposable {
		private readonly ErrorListProvider errors;
		private readonly Regex msvc = new Regex(@"^(?<file>.+)\((?<line>\d+)(?:,(?<column>\d+))?\)\s*:\s*(?<severity>fatal error|error|warning)\s*(?<message>.*)$");
		private readonly Regex clang = new Regex(@"^(?<file>.+):(?<line>\d+):(?<column>\d+):\s*(?<severity>fatal error|error|warning):\s*(?<message>.*)$");

		public KilnDiagnostics() {
			ThreadHelper.ThrowIfNotOnUIThread();
			errors = new ErrorListProvider(ServiceProvider.GlobalProvider) { ProviderName = "Kiln", ProviderGuid = new Guid("449240a8-545a-4734-a3cc-63718614fe11") };
		}

		public void Update(string root, string diagnostics) {
			ThreadHelper.ThrowIfNotOnUIThread();
			errors.Tasks.Clear();
			foreach (var line in diagnostics.Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries)) {
				var match = msvc.Match(line);
				if (!match.Success) {
					match = clang.Match(line);
				}
				if (!match.Success) {
					continue;
				}
				var path = match.Groups["file"].Value;
				var task = new ErrorTask {
					Category = TaskCategory.BuildCompile,
					ErrorCategory = match.Groups["severity"].Value == "warning" ? TaskErrorCategory.Warning : TaskErrorCategory.Error,
					Document = Path.IsPathRooted(path) ? path : Path.Combine(root, path),
					Line = int.Parse(match.Groups["line"].Value) - 1,
					Column = int.TryParse(match.Groups["column"].Value, out var column) ? Math.Max(0, column - 1) : 0,
					Text = match.Groups["message"].Value
				};
				task.Navigate += (sender, change) => {
					ThreadHelper.ThrowIfNotOnUIThread();
					VsShellUtilities.OpenDocument(ServiceProvider.GlobalProvider, task.Document, Guid.Empty, out _, out _, out var frame);
					VsShellUtilities.GetTextView(frame)?.SetCaretPos(task.Line, task.Column);
				};
				errors.Tasks.Add(task);
			}
			if (errors.Tasks.Count > 0) {
				errors.Show();
			}
		}

		public void Dispose() {
			errors.Dispose();
		}
	}
}
