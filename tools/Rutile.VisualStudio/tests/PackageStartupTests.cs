using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell;

namespace Rutile.VisualStudio {
	internal sealed class ShellContext : SynchronizationContext {
		private readonly Queue<Action> pending = new Queue<Action>();
		public override void Post(SendOrPostCallback callback, object? state) {
			pending.Enqueue(() => callback(state));
		}

		public void Drain() {
			while (pending.Count > 0) {
				pending.Dequeue()();
			}
		}
	}

	internal static class PackageStartupTests {
		private static void Require(bool condition, string message) {
			if (!condition) {
				throw new InvalidOperationException(message);
			}
		}

		public static int Main() {
			try {
				Run();
				Console.WriteLine("Kiln package startup checks passed.");
				return 0;
			}
			catch (Exception error) {
				Console.Error.WriteLine(error);
				return 1;
			}
		}

		private static void Run() {
			var context = new ShellContext();
			SynchronizationContext.SetSynchronizationContext(context);
			var activation = (ProvideAutoLoadAttribute)Attribute.GetCustomAttribute(typeof(KilnPackage), typeof(ProvideAutoLoadAttribute))!;
			Require(activation.Context == VSConstants.UICONTEXT.FolderOpened_string, "Folder opening must activate Kiln without a toolwindow click.");

			using (var package = new KilnPackage()) {
				var load = package.SetSiteAsync();
				Require(load.IsCompleted && !load.IsFaulted, "Package loading must finish before project discovery.");
				Require(AsyncPackage.FrameCreations == 0, "Package startup must not create a toolwindow frame.");
				Require(KilnWorkspaceControl.Created == 0, "Workspace startup must be deferred until SetSite completes.");
				Require(package.Commands.Registered.Count == 5 && package.PriorityCommands.Target != null, "Commands must be registered before a compiler request starts.");
				context.Drain();
				Require(KilnWorkspaceControl.Created == 1 && KilnWorkspaceControl.Started == 1, "Automatic folder activation must initialize exactly one workspace.");
				Require(!KilnWorkspaceControl.Discovery.Task.IsCompleted && load.IsCompleted, "A stalled project request must not block package loading.");

				var menu = package.Commands.Registered.Single(command => command.CommandID.ID == 0x0100);
				menu.Invoke();
				Require(AsyncPackage.FrameCreations == 1 && TestWindowFrame.Shown == 1, "The optional window must be creatable after package load.");
				Require(KilnWorkspaceControl.Created == 1 && KilnWorkspaceControl.Started == 1, "Opening the window must reuse the automatic workspace without another discovery request.");
				KilnWorkspaceControl.Last!.HasProject = true;
				var group = VSConstants.GUID_VSStandardCommandSet97;
				package.PriorityCommands.Target!.Exec(ref group, (uint)VSConstants.VSStd97CmdID.Start, 0, IntPtr.Zero, IntPtr.Zero);
				Require(KilnWorkspaceControl.Last.Operation == "debug", "F5 must use the automatic workspace even without toolwindow ownership.");
			}
			Require(KilnWorkspaceControl.Closed == 1, "Package disposal must release its workspace.");

			var created = KilnWorkspaceControl.Created;
			var canceled = new KilnPackage();
			canceled.SetSiteAsync().GetAwaiter().GetResult();
			canceled.Dispose();
			context.Drain();
			Require(KilnWorkspaceControl.Created == created, "Disposed packages must not start deferred workspace discovery.");

			KilnWorkspaceControl.Failure = new InvalidOperationException("Project discovery failed.");
			using (var package = new KilnPackage()) {
				package.SetSiteAsync().GetAwaiter().GetResult();
				context.Drain();
				Require(KilnOutput.Messages.Any(message => message.Contains("Project discovery failed.")), "Discovery failures must be reported without failing package load.");
				Require(package.Commands.Registered.Count == 5, "Project discovery failure must not remove registered commands.");
			}
		}
	}
}
