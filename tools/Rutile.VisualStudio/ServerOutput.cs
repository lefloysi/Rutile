using System;
using System.Diagnostics;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;

namespace Rutile.VisualStudio {
	internal static class ServerOutput {
		private static readonly Guid PaneId = new Guid("5c691b46-1bc0-4397-969e-61c8fe8c6c16");
		private static IVsOutputWindowPane? pane;

		public static void Write(string message) {
			Trace.WriteLine(message, "Rutile");
			_ = ThreadHelper.JoinableTaskFactory.RunAsync(async () => {
				await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
				if (pane == null && Package.GetGlobalService(typeof(SVsOutputWindow)) is IVsOutputWindow output) {
					var id = PaneId;
					output.CreatePane(ref id, "Rutile", 1, 0);
					output.GetPane(ref id, out pane);
				}
				pane?.OutputStringThreadSafe(message + Environment.NewLine);
			});
		}
	}
}
