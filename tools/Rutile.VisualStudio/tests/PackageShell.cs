using System;
using System.Collections.Generic;
using System.ComponentModel.Design;
using System.Threading;
using System.Threading.Tasks;

namespace System.Windows.Controls {
	public sealed class ContentControl {
		public object? Content { get; set; }
	}
}

namespace Microsoft.VisualStudio {
	public static class VSConstants {
		public const int S_OK = 0;
		public static readonly Guid GUID_VSStandardCommandSet97 = new Guid("5efc7975-14bc-11cf-9b2b-00aa00573819");
		public enum VSStd97CmdID {
			BuildSln,
			RebuildSln,
			Start,
			StartNoDebug,
		}
		public static class UICONTEXT {
			public const string FolderOpened_string = "4646b819-1ae0-4e79-97f4-8a8176fdd664";
		}
	}

	public static class ErrorHandler {
		public static void ThrowOnFailure(int result) {
			if (result < 0) {
				throw new InvalidOperationException("Shell operation failed.");
			}
		}

		public static bool Succeeded(int result) {
			return result >= 0;
		}
	}
}

namespace Microsoft.VisualStudio.OLE.Interop {
	public struct OLECMD {
		public uint cmdID;
		public uint cmdf;
	}
	public enum OLECMDF {
		OLECMDF_SUPPORTED = 1,
		OLECMDF_ENABLED = 2,
	}
	public enum Constants {
		OLECMDERR_E_NOTSUPPORTED = unchecked((int)0x80040100),
	}
	public interface IOleCommandTarget {
		int QueryStatus(ref Guid group, uint count, OLECMD[] commands, IntPtr text);
		int Exec(ref Guid group, uint id, uint options, IntPtr input, IntPtr output);
	}
}

namespace Microsoft.VisualStudio.Shell.Interop {
	public sealed class SVsRegisterPriorityCommandTarget {
	}
	public sealed class SVsShellDebugger {
	}
	public enum DBGMODE {
		DBGMODE_Design,
		DBGMODE_Run,
		DBGMODE_Break,
	}
	public interface IVsDebugger {
		int GetMode(DBGMODE[] mode);
	}
	public interface IVsWindowFrame {
		int Show();
	}
	public interface IVsRegisterPriorityCommandTarget {
		int RegisterPriorityCommandTarget(uint reserved, Microsoft.VisualStudio.OLE.Interop.IOleCommandTarget target, out uint cookie);
		int UnregisterPriorityCommandTarget(uint cookie);
	}
}

namespace Microsoft.VisualStudio.Shell {
	[AttributeUsage(AttributeTargets.Class)]
	public sealed class PackageRegistrationAttribute : Attribute {
		public bool UseManagedResourcesOnly { get; set; }
		public bool AllowsBackgroundLoading { get; set; }
	}
	[AttributeUsage(AttributeTargets.Class)]
	public sealed class ProvideMenuResourceAttribute : Attribute {
		public ProvideMenuResourceAttribute(string resource, int version) {
		}
	}
	[AttributeUsage(AttributeTargets.Class)]
	public sealed class ProvideToolWindowAttribute : Attribute {
		public ProvideToolWindowAttribute(Type type) {
		}
	}
	public enum PackageAutoLoadFlags {
		BackgroundLoad,
	}
	[AttributeUsage(AttributeTargets.Class)]
	public sealed class ProvideAutoLoadAttribute : Attribute {
		public string Context { get; }
		public ProvideAutoLoadAttribute(string context, PackageAutoLoadFlags flags) {
			Context = context;
		}
	}
	public struct ServiceProgressData {
	}
	public sealed class TestJoinableTaskFactory {
		public Task SwitchToMainThreadAsync(CancellationToken token = default) {
			token.ThrowIfCancellationRequested();
			return Task.CompletedTask;
		}
	}
	public static class ThreadHelper {
		public static void ThrowIfNotOnUIThread() {
		}
	}
	public class Package {
		public static object? GetGlobalService(Type service) {
			return null;
		}
	}
	public abstract class AsyncPackage : Package, IDisposable {
		public static bool SettingSite;
		public static int FrameCreations;
		public TestJoinableTaskFactory JoinableTaskFactory { get; } = new TestJoinableTaskFactory();
		public OleMenuCommandService Commands { get; } = new OleMenuCommandService();
		public TestPriorityCommands PriorityCommands { get; } = new TestPriorityCommands();

		public async Task SetSiteAsync() {
			SettingSite = true;
			try {
				await InitializeAsync(CancellationToken.None, null!);
			}
			finally {
				SettingSite = false;
			}
		}

		protected abstract Task InitializeAsync(CancellationToken token, IProgress<ServiceProgressData> progress);

		protected Task<object?> GetServiceAsync(Type service) {
			return Task.FromResult<object?>(service == typeof(IMenuCommandService) ? Commands : service == typeof(Interop.SVsRegisterPriorityCommandTarget) ? PriorityCommands : null);
		}

		protected ToolWindowPane FindToolWindow(Type type, int id, bool create) {
			if (SettingSite) {
				throw new InvalidOperationException("Creating a frame during SetSite recursively loads the package.");
			}
			FrameCreations++;
			return (ToolWindowPane)Activator.CreateInstance(type)!;
		}

		public void Dispose() {
			Dispose(true);
		}

		protected virtual void Dispose(bool disposing) {
		}
	}
	public sealed class TestPriorityCommands : Interop.IVsRegisterPriorityCommandTarget {
		public Microsoft.VisualStudio.OLE.Interop.IOleCommandTarget? Target;
		public int Unregistered;

		public int RegisterPriorityCommandTarget(uint reserved, Microsoft.VisualStudio.OLE.Interop.IOleCommandTarget target, out uint cookie) {
			Target = target;
			cookie = 1;
			return 0;
		}

		public int UnregisterPriorityCommandTarget(uint cookie) {
			Unregistered++;
			return 0;
		}
	}
	public class ToolWindowPane {
		public string? Caption { get; set; }
		public object? Content { get; set; }
		public object Frame { get; } = new TestWindowFrame();
		public ToolWindowPane(object? provider) {
		}
	}
	public sealed class TestWindowFrame : Interop.IVsWindowFrame {
		public static int Shown;
		public int Show() {
			Shown++;
			return 0;
		}
	}
	public sealed class OleMenuCmdEventArgs : EventArgs {
		public object? InValue { get; set; }
		public IntPtr OutValue { get; set; }
	}
	public sealed class OleMenuCommand : MenuCommand {
		public OleMenuCommand(EventHandler handler, CommandID id) : base(handler, id) {
		}
		public event EventHandler? BeforeQueryStatus;
		public void QueryStatus() {
			BeforeQueryStatus?.Invoke(this, EventArgs.Empty);
		}
	}
	public sealed class OleMenuCommandService {
		public List<MenuCommand> Registered { get; } = new List<MenuCommand>();
		public void AddCommand(MenuCommand command) {
			Registered.Add(command);
		}
	}
}

namespace Rutile.VisualStudio {
	internal sealed class KilnWorkspaceControl {
		public static int Created;
		public static int Started;
		public static int Closed;
		public static Exception? Failure;
		public static KilnWorkspaceControl? Last;
		public static TaskCompletionSource<bool> Discovery = new TaskCompletionSource<bool>();
		public bool HasProject { get; set; }
		public bool IsBusy { get; set; }
		public string SelectedProfile { get; set; } = "debug";
		public string SelectedTarget { get; set; } = "hello.exe";
		public string[] Profiles => new[] { "debug", "release" };
		public string[] Targets => new[] { "hello.exe" };
		public string? Operation;

		public KilnWorkspaceControl() {
			Created++;
			Last = this;
		}

		public Task InitializeAsync() {
			if (Microsoft.VisualStudio.Shell.AsyncPackage.SettingSite) {
				throw new InvalidOperationException("Project discovery started before SetSite finished.");
			}
			Started++;
			if (Failure != null) {
				throw Failure;
			}
			return Discovery.Task;
		}

		public void SelectProfile(string value) {
			SelectedProfile = value;
		}

		public void SelectTarget(string value) {
			SelectedTarget = value;
		}

		public void Invoke(string operation) {
			Operation = operation;
		}

		public void CloseWorkspace() {
			Closed++;
		}
	}
	internal static class KilnOutput {
		public static readonly List<string> Messages = new List<string>();
		public static void Write(string message, bool activate = false) {
			Messages.Add(message);
		}
	}
}
