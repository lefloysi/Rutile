using System;
using System.IO;
using System.Threading.Tasks;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.ComponentModelHost;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using Microsoft.VisualStudio.Workspace.VSIntegration.Contracts;

namespace Rutile.VisualStudio {
	internal sealed class WorkspaceRootMonitor : IVsSolutionEvents, IDisposable {
		private readonly Func<Task> changed;
		private readonly IVsFolderWorkspaceService? folders;
		private readonly IVsSolution? solution;
		private readonly uint cookie;
		private bool disposed;

		public WorkspaceRootMonitor(Func<Task> changed) {
			ThreadHelper.ThrowIfNotOnUIThread();
			this.changed = changed;
			var components = Package.GetGlobalService(typeof(SComponentModel)) as IComponentModel;
			folders = components?.GetService<IVsFolderWorkspaceService>();
			solution = Package.GetGlobalService(typeof(SVsSolution)) as IVsSolution;
			if (folders != null) {
				folders.OnActiveWorkspaceChanged += OnFolderChangedAsync;
			}
			if (solution != null) {
				ErrorHandler.ThrowOnFailure(solution.AdviseSolutionEvents(this, out cookie));
			}
		}

		public string? CurrentRoot() {
			ThreadHelper.ThrowIfNotOnUIThread();
			string? folder = folders?.CurrentWorkspace?.Location;
			if (string.IsNullOrEmpty(folder) && solution != null) {
				ErrorHandler.ThrowOnFailure(solution.GetSolutionInfo(out folder, out _, out _));
			}
			return string.IsNullOrEmpty(folder) || !Directory.Exists(folder) ? null : Path.GetFullPath(folder);
		}

		private async Task OnFolderChangedAsync(object? sender, EventArgs arguments) {
			if (disposed) {
				return;
			}
			try {
				await changed().ConfigureAwait(false);
			}
			catch (Exception error) {
				ServerOutput.Write(error.ToString());
			}
		}

		private void Changed() {
			if (disposed) {
				return;
			}
			_ = ThreadHelper.JoinableTaskFactory.RunAsync(async () => {
				try {
					await changed().ConfigureAwait(false);
				}
				catch (Exception error) {
					ServerOutput.Write(error.ToString());
				}
			});
		}

		public void Dispose() {
			ThreadHelper.ThrowIfNotOnUIThread();
			if (disposed) {
				return;
			}
			disposed = true;
			if (folders != null) {
				folders.OnActiveWorkspaceChanged -= OnFolderChangedAsync;
			}
			if (solution != null && cookie != 0) {
				ErrorHandler.ThrowOnFailure(solution.UnadviseSolutionEvents(cookie));
			}
		}

		public int OnAfterOpenSolution(object reserved, int newSolution) {
			Changed();
			return VSConstants.S_OK;
		}

		public int OnAfterCloseSolution(object reserved) {
			Changed();
			return VSConstants.S_OK;
		}

		public int OnAfterOpenProject(IVsHierarchy hierarchy, int added) {
			return VSConstants.S_OK;
		}

		public int OnQueryCloseProject(IVsHierarchy hierarchy, int removing, ref int cancel) {
			return VSConstants.S_OK;
		}

		public int OnBeforeCloseProject(IVsHierarchy hierarchy, int removing) {
			return VSConstants.S_OK;
		}

		public int OnAfterLoadProject(IVsHierarchy stub, IVsHierarchy real) {
			return VSConstants.S_OK;
		}

		public int OnQueryUnloadProject(IVsHierarchy hierarchy, ref int cancel) {
			return VSConstants.S_OK;
		}

		public int OnBeforeUnloadProject(IVsHierarchy real, IVsHierarchy stub) {
			return VSConstants.S_OK;
		}

		public int OnQueryCloseSolution(object reserved, ref int cancel) {
			return VSConstants.S_OK;
		}

		public int OnBeforeCloseSolution(object reserved) {
			return VSConstants.S_OK;
		}
	}
}
