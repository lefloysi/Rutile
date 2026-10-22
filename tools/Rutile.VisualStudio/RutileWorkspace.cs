using System;
using System.IO;
using System.Threading.Tasks;
using Microsoft.VisualStudio.Shell;

namespace Rutile.VisualStudio {
	internal sealed class RutileWorkspace : IDisposable {
		private WorkspaceRootMonitor? roots;
		public event EventHandler? RootChanged;

		internal static string FindCompiler(string extensionDirectory, string? configured) {
			if (!string.IsNullOrWhiteSpace(configured)) {
				if (!File.Exists(configured)) {
					throw new FileNotFoundException("RUTILE_COMPILER does not name an existing Rutile compiler executable.", configured);
				}
				return Path.GetFullPath(configured);
			}
			var local = Path.Combine(extensionDirectory, "rutilec.exe");
			if (File.Exists(local)) {
				return local;
			}
			throw new FileNotFoundException("No Rutile compiler was found. Set RUTILE_COMPILER or place rutilec.exe beside the extension.");
		}

		public string? CurrentRoot() {
			ThreadHelper.ThrowIfNotOnUIThread();
			roots ??= new WorkspaceRootMonitor(OnRootChangedAsync);
			return roots.CurrentRoot();
		}

		private Task OnRootChangedAsync() {
			RootChanged?.Invoke(this, EventArgs.Empty);
			return Task.CompletedTask;
		}

		public void Dispose() {
			ThreadHelper.ThrowIfNotOnUIThread();
			roots?.Dispose();
			roots = null;
			RootChanged = null;
		}
	}
}
