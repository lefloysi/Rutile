using System;
using System.ComponentModel.Design;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Controls;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using Microsoft.VisualStudio.OLE.Interop;

namespace Rutile.VisualStudio {
	[PackageRegistration(UseManagedResourcesOnly = true, AllowsBackgroundLoading = true)]
	[ProvideMenuResource("Menus.ctmenu", 1)]
	[ProvideToolWindow(typeof(KilnWindow))]
	[ProvideAutoLoad(VSConstants.UICONTEXT.FolderOpened_string, PackageAutoLoadFlags.BackgroundLoad)]
	[Guid("721e7bea-9405-4637-b2db-c903724ef228")]
	public sealed class KilnPackage : AsyncPackage {
		private IVsRegisterPriorityCommandTarget? commandTargets;
		private uint commandCookie;
		private KilnWorkspaceControl? workspace;
		private readonly CancellationTokenSource lifetime = new CancellationTokenSource();
		private Task? workspaceInitialization;
		private Exception? initializationError;

		protected override async Task InitializeAsync(CancellationToken cancellationToken, IProgress<ServiceProgressData> progress) {
			await JoinableTaskFactory.SwitchToMainThreadAsync(cancellationToken);
			if (await GetServiceAsync(typeof(IMenuCommandService)) is OleMenuCommandService commands) {
				RegisterCombo(commands, 0x0200, 0x0201, () => workspace?.SelectedProfile ?? "", () => workspace?.Profiles ?? Array.Empty<string>(), value => workspace?.SelectProfile(value));
				RegisterCombo(commands, 0x0202, 0x0203, () => workspace?.SelectedTarget ?? "", () => workspace?.Targets ?? Array.Empty<string>(), value => workspace?.SelectTarget(value));
				commands.AddCommand(new MenuCommand((sender, arguments) => ShowKilnWindow(), new CommandID(new Guid("9b0e8924-199b-4da6-a104-841eb588f058"), 0x0100)));
			}
			commandTargets = await GetServiceAsync(typeof(SVsRegisterPriorityCommandTarget)) as IVsRegisterPriorityCommandTarget;
			if (commandTargets != null) {
				ErrorHandler.ThrowOnFailure(commandTargets.RegisterPriorityCommandTarget(0, new KilnCommandTarget(() => workspace), out commandCookie));
			}
			StartWorkspace();
		}

		private void StartWorkspace() {
			ThreadHelper.ThrowIfNotOnUIThread();
			if (lifetime.IsCancellationRequested || (workspaceInitialization != null && (initializationError == null || !workspaceInitialization.IsCompleted))) {
				return;
			}
			workspaceInitialization = InitializeWorkspaceAsync(lifetime.Token);
		}

		private async Task InitializeWorkspaceAsync(CancellationToken token) {
			try {
				// Package SetSite must finish before workspace services or project requests can run.
				await Task.Yield();
				await JoinableTaskFactory.SwitchToMainThreadAsync(token);
				workspace ??= new KilnWorkspaceControl();
				await workspace.InitializeAsync();
				initializationError = null;
			}
			catch (OperationCanceledException) when (token.IsCancellationRequested) {
			}
			catch (Exception error) {
				initializationError = error;
				KilnOutput.Write("Kiln project initialization failed: " + error, true);
			}
		}

		private void ShowKilnWindow() {
			ThreadHelper.ThrowIfNotOnUIThread();
			try {
				workspace ??= new KilnWorkspaceControl();
				var window = (KilnWindow)FindToolWindow(typeof(KilnWindow), 0, true);
				window.Attach(workspace);
				ErrorHandler.ThrowOnFailure(((IVsWindowFrame)window.Frame).Show());
				StartWorkspace();
			}
			catch (Exception error) {
				KilnOutput.Write("Could not open the Kiln window: " + error, true);
			}
		}

		private void RegisterCombo(OleMenuCommandService commands, int selectionId, int listId, Func<string> selected, Func<string[]> choices, Action<string> select) {
			ThreadHelper.ThrowIfNotOnUIThread();
			var group = new Guid("9b0e8924-199b-4da6-a104-841eb588f058");
			var selection = new OleMenuCommand((sender, arguments) => {
				ThreadHelper.ThrowIfNotOnUIThread();
				if (arguments is OleMenuCmdEventArgs request) {
					if (request.InValue is string value) {
						select(value);
					}
					if (request.OutValue != IntPtr.Zero) {
						Marshal.GetNativeVariantForObject(selected(), request.OutValue);
					}
				}
			}, new CommandID(group, selectionId));
			selection.BeforeQueryStatus += (sender, arguments) => {
				selection.Visible = workspace?.HasProject == true;
				selection.Enabled = workspace?.HasProject == true && workspace?.IsBusy == false;
			};
			commands.AddCommand(selection);
			commands.AddCommand(new OleMenuCommand((sender, arguments) => {
				if (arguments is OleMenuCmdEventArgs request && request.OutValue != IntPtr.Zero) {
					Marshal.GetNativeVariantForObject(choices(), request.OutValue);
				}
			}, new CommandID(group, listId)));
		}

		protected override void Dispose(bool disposing) {
			ThreadHelper.ThrowIfNotOnUIThread();
			lifetime.Cancel();
			workspace?.CloseWorkspace();
			if (commandTargets != null && commandCookie != 0) {
				commandTargets.UnregisterPriorityCommandTarget(commandCookie);
				commandCookie = 0;
			}
			base.Dispose(disposing);
			lifetime.Dispose();
		}
	}

	internal sealed class KilnCommandTarget : IOleCommandTarget {
		private readonly Func<KilnWorkspaceControl?> workspace;

		public KilnCommandTarget(Func<KilnWorkspaceControl?> workspace) {
			this.workspace = workspace;
		}

		private string? Operation(Guid group, uint id) {
			ThreadHelper.ThrowIfNotOnUIThread();
			if (group != VSConstants.GUID_VSStandardCommandSet97 || workspace()?.HasProject != true) {
				return null;
			}
			var mode = new DBGMODE[1];
			if (Package.GetGlobalService(typeof(SVsShellDebugger)) is IVsDebugger debugger && ErrorHandler.Succeeded(debugger.GetMode(mode)) && mode[0] != DBGMODE.DBGMODE_Design) {
				return null;
			}
			switch ((VSConstants.VSStd97CmdID)id) {
				case VSConstants.VSStd97CmdID.BuildSln:
				case VSConstants.VSStd97CmdID.RebuildSln:
					return "build";
				case VSConstants.VSStd97CmdID.Start:
					return "debug";
				case VSConstants.VSStd97CmdID.StartNoDebug:
					return "run";
			}
			return null;
		}

		public int QueryStatus(ref Guid group, uint count, OLECMD[] commands, IntPtr text) {
			ThreadHelper.ThrowIfNotOnUIThread();
			var handled = false;
			for (var index = 0; index < count; index++) {
				if (Operation(group, commands[index].cmdID) == null) {
					continue;
				}
				commands[index].cmdf = (uint)OLECMDF.OLECMDF_SUPPORTED | (workspace()?.IsBusy == true ? 0 : (uint)OLECMDF.OLECMDF_ENABLED);
				handled = true;
			}
			return handled ? VSConstants.S_OK : (int)Microsoft.VisualStudio.OLE.Interop.Constants.OLECMDERR_E_NOTSUPPORTED;
		}

		public int Exec(ref Guid group, uint id, uint options, IntPtr input, IntPtr output) {
			ThreadHelper.ThrowIfNotOnUIThread();
			var operation = Operation(group, id);
			var control = workspace();
			if (operation == null || control == null || control.IsBusy) {
				return (int)Microsoft.VisualStudio.OLE.Interop.Constants.OLECMDERR_E_NOTSUPPORTED;
			}
			control.Invoke(operation);
			return VSConstants.S_OK;
		}
	}

	[Guid("ad11253d-320a-45ab-88c8-dce1bb5194bf")]
	public sealed class KilnWindow : ToolWindowPane {
		private readonly ContentControl content = new ContentControl();

		public KilnWindow() : base(null) {
			Caption = "Kiln";
			Content = content;
		}

		internal void Attach(KilnWorkspaceControl workspace) {
			content.Content = workspace;
		}
	}
}
