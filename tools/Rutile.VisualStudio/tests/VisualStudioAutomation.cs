using System;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;

public static class VisualStudioAutomation {
	[DllImport("ole32.dll")]
	private static extern int GetRunningObjectTable(int reserved, out IRunningObjectTable table);

	[DllImport("ole32.dll")]
	private static extern int CreateBindCtx(int reserved, out IBindCtx context);

	public static object Find(int processId) {
		IRunningObjectTable table;
		IBindCtx context;
		Marshal.ThrowExceptionForHR(GetRunningObjectTable(0, out table));
		Marshal.ThrowExceptionForHR(CreateBindCtx(0, out context));
		try {
			IEnumMoniker names;
			table.EnumRunning(out names);
			try {
				var item = new IMoniker[1];
				while (names.Next(1, item, IntPtr.Zero) == 0) {
					try {
						string name;
						item[0].GetDisplayName(context, null, out name);
						if (name.StartsWith("!VisualStudio.DTE.", StringComparison.Ordinal) && name.EndsWith(":" + processId, StringComparison.Ordinal)) {
							object result;
							table.GetObject(item[0], out result);
							return result;
						}
					}
					finally {
						Marshal.ReleaseComObject(item[0]);
					}
				}
			}
			finally {
				Marshal.ReleaseComObject(names);
			}
		}
		finally {
			Marshal.ReleaseComObject(context);
			Marshal.ReleaseComObject(table);
		}
		return null;
	}
}

[ComImport, Guid("00000016-0000-0000-C000-000000000046"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface AutomationMessageFilter {
	[PreserveSig]
	int HandleInComingCall(int type, IntPtr caller, int tick, IntPtr information);

	[PreserveSig]
	int RetryRejectedCall(IntPtr callee, int tick, int rejectType);

	[PreserveSig]
	int MessagePending(IntPtr callee, int tick, int pendingType);
}

public sealed class VisualStudioMessageFilter : AutomationMessageFilter, IDisposable {
	private AutomationMessageFilter previous;

	[DllImport("ole32.dll")]
	private static extern int CoRegisterMessageFilter(AutomationMessageFilter filter, out AutomationMessageFilter previous);

	public VisualStudioMessageFilter() {
		Marshal.ThrowExceptionForHR(CoRegisterMessageFilter(this, out previous));
	}

	public int HandleInComingCall(int type, IntPtr caller, int tick, IntPtr information) {
		return 0;
	}

	public int RetryRejectedCall(IntPtr callee, int tick, int rejectType) {
		return rejectType == 2 && tick < 5000 ? 100 : -1;
	}

	public int MessagePending(IntPtr callee, int tick, int pendingType) {
		return 2;
	}

	public void Dispose() {
		AutomationMessageFilter unused;
		Marshal.ThrowExceptionForHR(CoRegisterMessageFilter(previous, out unused));
	}
}

public sealed class OwnedProcessTimeout : IDisposable {
	private readonly System.Threading.Timer timer;

	public OwnedProcessTimeout(System.Diagnostics.Process process, int milliseconds) {
		timer = new System.Threading.Timer(_ => {
			try {
				if (!process.HasExited) {
					process.Kill();
				}
			}
			catch (InvalidOperationException) {
			}
			catch (System.ComponentModel.Win32Exception) {
			}
		}, null, milliseconds, System.Threading.Timeout.Infinite);
	}

	public void Dispose() {
		timer.Dispose();
	}
}
