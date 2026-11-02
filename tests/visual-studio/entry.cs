using System;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

public static class EditorObservationEntry {
	public static int Invoke(string argument) {
		try {
			var separator = argument.IndexOf(':');
			var destination = new IntPtr(long.Parse(argument.Substring(0, separator), CultureInfo.InvariantCulture));
			var assembly = typeof(EditorObservationEntry).Assembly.Location;
			var setup = new AppDomainSetup {
				ApplicationBase = Path.GetDirectoryName(assembly),
				ConfigurationFile = assembly + ".config"
			};
			var domain = AppDomain.CreateDomain("Editor observation", null, setup);
			ResolveEventHandler resolve_entry = (sender, request) => request.Name == typeof(EditorObservationEntry).Assembly.FullName ? typeof(EditorObservationEntry).Assembly : null;
			AppDomain.CurrentDomain.AssemblyResolve += resolve_entry;
			string output;
			try {
				var runner = (EditorObservationRunner)domain.CreateInstanceFromAndUnwrap(assembly, typeof(EditorObservationRunner).FullName);
				output = runner.Observe(argument.Substring(separator + 1));
			}
			finally {
				AppDomain.CurrentDomain.AssemblyResolve -= resolve_entry;
				AppDomain.Unload(domain);
			}
			var bytes = Encoding.UTF8.GetBytes(output + "\0");
			var allocation = Marshal.AllocCoTaskMem(bytes.Length);
			Marshal.Copy(bytes, 0, allocation, bytes.Length);
			Marshal.WriteIntPtr(destination, allocation);
			return 0;
		}
		catch (Exception error) {
			Console.Error.WriteLine((error.InnerException ?? error).ToString());
			return 1;
		}
	}
}

public sealed class EditorObservationRunner : MarshalByRefObject {
	public string Observe(string name) {
		var method = typeof(EditorObservations).GetMethod(name, BindingFlags.Public | BindingFlags.Static, null, Type.EmptyTypes, null);
		if (method == null) {
			throw new MissingMethodException(typeof(EditorObservations).FullName, name);
		}
		return (string)method.Invoke(null, null);
	}
}
