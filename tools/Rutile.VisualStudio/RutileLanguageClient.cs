using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio.LanguageServer.Client;
using Kiln.VisualStudio;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Threading;
using Microsoft.VisualStudio.Utilities;
using Newtonsoft.Json.Linq;
using StreamJsonRpc;

namespace Rutile.VisualStudio {
	[Export(typeof(ILanguageClient)), ContentType(RutileContentType.Name), Name("Rutile Language Client")]
	internal sealed class RutileLanguageClient : ILanguageClient, ILanguageClientCustomMessage2, IDisposable {
		private ServerProcess? server;
		private readonly IKilnWorkspace workspace;
		private int subscribed;
		private string? serverRoot;
		private int disposed;
		private int initialized;

		[ImportingConstructor]
		public RutileLanguageClient(IKilnWorkspace workspace) {
			this.workspace = workspace;
		}

		public string Name => "Rutile";
		public IEnumerable<string> ConfigurationSections => Array.Empty<string>();
		public object InitializationOptions => new JObject { ["rutileSemanticColors"] = true };
		public IEnumerable<string> FilesToWatch => new[] { "**/*.rt", "**/*.list" };
		public bool ShowNotificationOnInitializeFailed => true;
		public object MiddleLayer { get; } = new RutileSemanticColors(RutileSemanticStore.Instance);
		public object CustomMessageTarget => RutileEditorConnection.Instance;

		public Task AttachForCustomMessageAsync(JsonRpc rpc) {
			RutileEditorConnection.Instance.Attach(rpc);
			return Volatile.Read(ref initialized) != 0 ? RutileSemanticStore.Instance.InitializeAsync() : Task.CompletedTask;
		}

		public event AsyncEventHandler<EventArgs>? StartAsync;
		public event AsyncEventHandler<EventArgs>? StopAsync;

		public async Task<Connection?> ActivateAsync(CancellationToken token) {
			token.ThrowIfCancellationRequested();
			Volatile.Write(ref initialized, 0);
			await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync(token);
			if (Volatile.Read(ref disposed) != 0) {
				throw new ObjectDisposedException(nameof(RutileLanguageClient));
			}
			EnsureRootMonitor();
			var folder = workspace.CurrentRoot();
			if (folder == null) {
				throw new InvalidOperationException("Open a Rutile folder or solution before using Rutile language support.");
			}

			StopServer();
			var created = new ServerProcess(workspace.ExecutablePath, folder, token, OnExitedAsync, ServerOutput.Write);
			serverRoot = folder;
			Interlocked.Exchange(ref server, created)?.Dispose();

			return created.Connection;
		}

		public async Task OnLoadedAsync() {
			await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
			EnsureRootMonitor();
			if (workspace.CurrentRoot() == null) {
				return;
			}
			var handler = StartAsync;

			if (handler != null) {
				await handler.InvokeAsync(this, EventArgs.Empty).ConfigureAwait(false);
			}
		}

		public Task OnServerInitializedAsync() {
			Volatile.Write(ref initialized, 1);
			return RutileSemanticStore.Instance.InitializeAsync();
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
			if (Interlocked.Exchange(ref subscribed, 1) == 0) {
				workspace.RootChanged += OnWorkspaceRootChanged;
			}
		}

		private void OnWorkspaceRootChanged(object? sender, EventArgs arguments) {
			_ = ThreadHelper.JoinableTaskFactory.RunAsync(OnRootChangedAsync);
		}

		private async Task OnRootChangedAsync() {
			await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
			if (Volatile.Read(ref disposed) != 0) {
				return;
			}
			var folder = workspace.CurrentRoot();
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
			if (Interlocked.Exchange(ref subscribed, 0) != 0) {
				workspace.RootChanged -= OnWorkspaceRootChanged;
			}
		}
	}
}
