using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio.LanguageServer.Client;
using Microsoft.VisualStudio.ComponentModelHost;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using Microsoft.VisualStudio.Workspace.VSIntegration.Contracts;
using Microsoft.VisualStudio.Threading;
using Microsoft.VisualStudio.Utilities;

namespace Rutile.VisualStudio {
	[Export(typeof(ILanguageClient)), ContentType(RutileContentType.Name), Name("Rutile Language Client")]
	internal sealed class RutileLanguageClient : ILanguageClient, IDisposable {
		private ServerProcess? server;

		public string Name => "Rutile";
		public IEnumerable<string> ConfigurationSections => Array.Empty<string>();
		public object InitializationOptions => null!;
		public IEnumerable<string> FilesToWatch => Array.Empty<string>();
		public bool ShowNotificationOnInitializeFailed => true;
		public event AsyncEventHandler<EventArgs>? StartAsync;
		public event AsyncEventHandler<EventArgs>? StopAsync;

		public async Task<Connection?> ActivateAsync(CancellationToken token) {
			token.ThrowIfCancellationRequested();
			Dispose();

			var directory = Path.GetDirectoryName(typeof(RutileLanguageClient).Assembly.Location)!;
			await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync(token);
			string? folder = null;
			var components = Package.GetGlobalService(typeof(SComponentModel)) as IComponentModel;
			var workspace = components?.GetService<IVsFolderWorkspaceService>()?.CurrentWorkspace;
			if (workspace != null) {
				folder = workspace.Location;
			}
			if (string.IsNullOrEmpty(folder) && Package.GetGlobalService(typeof(SVsSolution)) is IVsSolution solution) {
				solution.GetSolutionInfo(out folder, out _, out _);
			}
			if (string.IsNullOrEmpty(folder) || !Directory.Exists(folder)) {
				throw new InvalidOperationException("Open a Rutile folder or solution before using Rutile language support.");
			}

			var created = new ServerProcess(Path.Combine(directory, "kiln.exe"), Path.GetFullPath(folder), token, OnExitedAsync);
			Interlocked.Exchange(ref server, created)?.Dispose();

			return created.Connection;
		}

		public async Task OnLoadedAsync() {
			var handler = StartAsync;

			if (handler != null) {
				await handler.InvokeAsync(this, EventArgs.Empty).ConfigureAwait(false);
			}
		}

		public Task OnServerInitializedAsync() {
			return Task.CompletedTask;
		}

		public Task<InitializationFailureContext?> OnServerInitializeFailedAsync(ILanguageClientInitializationInfo information) {
			if (information.InitializationException != null) {
				ServerOutput.Write(information.InitializationException.ToString());
			}

			Dispose();
			return Task.FromResult<InitializationFailureContext?>(new InitializationFailureContext {
				FailureMessage = "Rutile language support could not start. See the Rutile Output pane for details."
			});
		}

		private async Task OnExitedAsync() {
			var handler = StopAsync;

			if (handler != null) {
				await handler.InvokeAsync(this, EventArgs.Empty).ConfigureAwait(false);
			}
		}

		public void Dispose() {
			Interlocked.Exchange(ref server, null)?.Dispose();
		}
	}
}
