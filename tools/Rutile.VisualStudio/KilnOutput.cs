using System;
using System.Diagnostics;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;

namespace Rutile.VisualStudio {
	internal static class KilnOutput {
		private static readonly Guid PaneId = new Guid("b10230de-7069-439f-8e27-d60aa1b7f1c4");
		private static IVsOutputWindowPane? pane;

		public static void Write(string message, bool activate = false) {
			Trace.WriteLine(message, "Kiln");
			_ = ThreadHelper.JoinableTaskFactory.RunAsync(async () => {
				await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
				if (pane == null && Package.GetGlobalService(typeof(SVsOutputWindow)) is IVsOutputWindow output) {
					var id = PaneId;
					output.CreatePane(ref id, "Kiln", 1, 0);
					output.GetPane(ref id, out pane);
				}
				pane?.OutputStringThreadSafe(message + Environment.NewLine);
				if (activate) {
					pane?.Activate();
					if (Package.GetGlobalService(typeof(SVsUIShell)) is IVsUIShell shell) {
						var outputId = VSConstants.StandardToolWindows.Output;
						if (ErrorHandler.Succeeded(shell.FindToolWindow((uint)__VSFINDTOOLWIN.FTW_fForceCreate, ref outputId, out var frame))) {
							frame.Show();
						}
					}
				}
			});
		}
	}
}
