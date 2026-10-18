using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio.LanguageServer.Client;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Threading;
using Microsoft.VisualStudio.Utilities;

namespace Rutile.VisualStudio {
	[Export(typeof(ILanguageClient)), ContentType(RutileContentType.Name), Name("Rutile Language Client")]
	internal sealed class RutileLanguageClient : ILanguageClient, IDisposable {
		private ServerProcess? server;
		private WorkspaceRootMonitor? roots;
		private string? serverRoot;
		private int disposed;

		public string Name => "Rutile";
		public IEnumerable<string> ConfigurationSections => Array.Empty<string>();
		public object InitializationOptions => null!;
		public IEnumerable<string> FilesToWatch => new[] { "**/*.rt", "**/rutile.yaml" };
		public bool ShowNotificationOnInitializeFailed => true;
		public event AsyncEventHandler<EventArgs>? StartAsync;
		public event AsyncEventHandler<EventArgs>? StopAsync;

		public async Task<Connection?> ActivateAsync(CancellationToken token) {
			token.ThrowIfCancellationRequested();
			var directory = Path.GetDirectoryName(typeof(RutileLanguageClient).Assembly.Location)!;
			await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync(token);
			if (Volatile.Read(ref disposed) != 0) {
				throw new ObjectDisposedException(nameof(RutileLanguageClient));
			}
			EnsureRootMonitor();
			var folder = roots!.CurrentRoot();
			if (folder == null) {
				throw new InvalidOperationException("Open a Rutile folder or solution before using Rutile language support.");
			}

			StopServer();
			var created = new ServerProcess(Path.Combine(directory, "kiln.exe"), folder, token, OnExitedAsync, ServerOutput.Write);
			serverRoot = folder;
			Interlocked.Exchange(ref server, created)?.Dispose();

			return created.Connection;
		}

		public async Task OnLoadedAsync() {
			await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
			EnsureRootMonitor();
			if (roots!.CurrentRoot() == null) {
				return;
			}
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

			StopServer();
			return Task.FromResult<InitializationFailureContext?>(new InitializationFailureContext {
				FailureMessage = "Rutile language support could not start. See the Rutile Output pane for details."
			});
		}

		private async Task OnExitedAsync(ServerProcess exited) {
			await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
			if (Interlocked.CompareExchange(ref server, null, exited) != exited) {
				return;
			}
			serverRoot = null;
			exited.Dispose();
			var handler = StopAsync;

			if (handler != null) {
				await handler.InvokeAsync(this, EventArgs.Empty).ConfigureAwait(false);
			}
		}

		private void EnsureRootMonitor() {
			ThreadHelper.ThrowIfNotOnUIThread();
			if (roots == null) {
				roots = new WorkspaceRootMonitor(OnRootChangedAsync);
			}
		}

		private async Task OnRootChangedAsync() {
			await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
			if (Volatile.Read(ref disposed) != 0 || roots == null) {
				return;
			}
			var folder = roots.CurrentRoot();
			if (server != null && string.Equals(folder, serverRoot, StringComparison.OrdinalIgnoreCase)) {
				return;
			}

			StopServer();
			var stop = StopAsync;
			if (stop != null) {
				await stop.InvokeAsync(this, EventArgs.Empty).ConfigureAwait(false);
			}
			if (folder == null || Volatile.Read(ref disposed) != 0) {
				return;
			}
			var start = StartAsync;
			if (start != null) {
				await start.InvokeAsync(this, EventArgs.Empty).ConfigureAwait(false);
			}
		}

		private void StopServer() {
			serverRoot = null;
			Interlocked.Exchange(ref server, null)?.Dispose();
		}

		public void Dispose() {
			if (Interlocked.Exchange(ref disposed, 1) != 0) {
				return;
			}
			StopServer();
			var monitor = Interlocked.Exchange(ref roots, null);
			if (monitor != null) {
				_ = ThreadHelper.JoinableTaskFactory.RunAsync(async () => {
					await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
					monitor.Dispose();
				});
			}
		}
	}
}
