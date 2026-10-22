using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Security.Principal;
using System.Text;
using Microsoft.Win32;

internal sealed class InstallManifest {
	internal string Name;
	internal string Version;
	internal string Scope;
	internal bool AddedPath;
	internal Dictionary<string, string> Files = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
}

internal static class Setup {
	private const string ManifestName = "install-manifest.txt";
	private static string product;
	private static string version;
	private static readonly UTF8Encoding Utf8 = new UTF8Encoding(false, true);

	[DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
	private static extern IntPtr SendMessageTimeout(IntPtr window, uint message, UIntPtr parameter, string text, uint flags, uint timeout, out UIntPtr result);

	private static int Main(string[] args) {
		try {
			using (StreamReader metadata = new StreamReader(Assembly.GetExecutingAssembly().GetManifestResourceStream("product.txt"), Utf8)) {
				product = metadata.ReadLine();
				version = metadata.ReadLine();
			}
			bool uninstall = args.Contains("uninstall") || args.Contains("--uninstall");
			bool install = args.Contains("install") || args.Contains("--install");
			if (args.Contains("--help") || args.Contains("-h")) {
				Usage();
				return 0;
			}
			if (install == uninstall) {
				Usage();
				return 1;
			}
			bool machine = false;
			bool specifiedScope = false;
			string prefix = null;
			for (int index = 0; index < args.Length; ++index) {
				string argument = args[index];
				if (argument == "install" || argument == "uninstall" || argument == "--install" || argument == "--uninstall") {
					continue;
				}
				if (argument == "--user" || argument == "--machine") {
					bool selectedMachine = argument == "--machine";
					if (specifiedScope && machine != selectedMachine) {
						throw new InvalidOperationException("Choose either --user or --machine.");
					}
					machine = selectedMachine;
					specifiedScope = true;
					continue;
				}
				if (argument == "--prefix" && index + 1 < args.Length) {
					prefix = args[++index];
					continue;
				}
				throw new InvalidOperationException("Unknown or incomplete option: " + argument);
			}
			string executable = Assembly.GetExecutingAssembly().Location;
			if (uninstall && prefix == null && string.Equals(Path.GetFileName(executable), "uninstall.exe", StringComparison.OrdinalIgnoreCase)) {
				prefix = Path.GetDirectoryName(executable);
			}
			if (prefix == null) {
				prefix = machine ? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), product)
					: Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Programs", product);
			}
			prefix = Path.GetFullPath(prefix).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
			if (string.Equals(prefix + Path.DirectorySeparatorChar, Path.GetPathRoot(prefix), StringComparison.OrdinalIgnoreCase)) {
				throw new InvalidOperationException("The installation prefix must be a product directory, not a drive root.");
			}
			RejectLinks(prefix);
			InstallManifest previous = ReadManifest(prefix);
			if (previous != null) {
				if (previous.Name != product) {
					throw new InvalidOperationException("This directory belongs to another installed product.");
				}
				if (specifiedScope && machine != (previous.Scope == "machine")) {
					throw new InvalidOperationException("The requested scope differs from the existing installation.");
				}
				machine = previous.Scope == "machine";
			}
			if (machine && !new WindowsPrincipal(WindowsIdentity.GetCurrent()).IsInRole(WindowsBuiltInRole.Administrator)) {
				throw new InvalidOperationException("Machine installation requires an administrator command prompt. Use --user for your account.");
			}
			if (uninstall) {
				Uninstall(prefix, previous, machine);
			}
			else {
				Install(prefix, previous, machine);
			}
			return 0;
		}
		catch (Exception error) {
			Console.Error.WriteLine(error.Message);
			return 1;
		}
	}

	private static void Usage() {
		Console.WriteLine(product + " " + version + " command-line installer");
		Console.WriteLine("Usage: " + product + "Setup.exe install [--user | --machine] [--prefix DIRECTORY]");
		Console.WriteLine("       " + product + "Setup.exe uninstall [--user | --machine] [--prefix DIRECTORY]");
		Console.WriteLine("Default scope: current user. Restart your terminal and editor after installation.");
	}

	private static string Within(string root, string relative) {
		if (string.IsNullOrEmpty(relative) || Path.IsPathRooted(relative) || relative.IndexOf(':') >= 0) {
			throw new InvalidOperationException("Invalid installation file path.");
		}
		string path = Path.GetFullPath(Path.Combine(root, relative.Replace('/', Path.DirectorySeparatorChar)));
		if (!path.StartsWith(root.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase)) {
			throw new InvalidOperationException("An installation file escapes the product directory.");
		}
		RejectLinks(path);
		return path;
	}

	private static void RejectLinks(string path) {
		string current = path;
		while (!string.IsNullOrEmpty(current)) {
			if ((File.Exists(current) || Directory.Exists(current)) && (File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0) {
				throw new InvalidOperationException("Installation paths must not pass through a symbolic link or junction: " + current);
			}
			current = Path.GetDirectoryName(current);
		}
	}

	private static string Hash(string path) {
		using (SHA256 hash = SHA256.Create()) {
			using (FileStream input = File.OpenRead(path)) {
				return BitConverter.ToString(hash.ComputeHash(input)).Replace("-", "");
			}
		}
	}

	private static InstallManifest ReadManifest(string root) {
		string path = Path.Combine(root, ManifestName);
		if (!File.Exists(path)) {
			return null;
		}
		string[] lines = File.ReadAllLines(path, Utf8);
		if (lines.Length < 4 || (lines[2] != "user" && lines[2] != "machine")) {
			throw new InvalidOperationException("The installation manifest is invalid.");
		}
		InstallManifest manifest = new InstallManifest { Name = lines[0], Version = lines[1], Scope = lines[2], AddedPath = bool.Parse(lines[3]) };
		for (int index = 4; index < lines.Length; ++index) {
			string[] entry = lines[index].Split('\t');
			if (entry.Length != 2 || entry[1].Length != 64) {
				throw new InvalidOperationException("The installation file manifest is invalid.");
			}
			string relative = Utf8.GetString(Convert.FromBase64String(entry[0]));
			Within(root, relative);
			manifest.Files.Add(relative, entry[1]);
		}
		return manifest;
	}

	private static void WriteManifest(string root, InstallManifest manifest) {
		List<string> lines = new List<string> { manifest.Name, manifest.Version, manifest.Scope, manifest.AddedPath.ToString() };
		foreach (KeyValuePair<string, string> file in manifest.Files.OrderBy(entry => entry.Key, StringComparer.OrdinalIgnoreCase)) {
			lines.Add(Convert.ToBase64String(Utf8.GetBytes(file.Key)) + "\t" + file.Value);
		}
		string path = Path.Combine(root, ManifestName);
		File.WriteAllLines(path + ".new", lines, Utf8);
		if (File.Exists(path)) {
			File.Replace(path + ".new", path, null);
		}
		else {
			File.Move(path + ".new", path);
		}
	}

	private static void Install(string root, InstallManifest previous, bool machine) {
		string staging = Path.Combine(Path.GetTempPath(), product + "-setup-" + Guid.NewGuid().ToString("N"));
		Directory.CreateDirectory(staging);
		try {
			InstallManifest manifest = new InstallManifest { Name = product, Version = version, Scope = machine ? "machine" : "user", AddedPath = previous != null && previous.AddedPath };
			using (Stream resource = Assembly.GetExecutingAssembly().GetManifestResourceStream("payload.zip")) {
				using (ZipArchive archive = new ZipArchive(resource, ZipArchiveMode.Read)) {
					foreach (ZipArchiveEntry entry in archive.Entries) {
						if (entry.FullName.EndsWith("/", StringComparison.Ordinal)) {
							continue;
						}
						string relative = entry.FullName.Replace('/', Path.DirectorySeparatorChar);
						if (relative.Equals(ManifestName, StringComparison.OrdinalIgnoreCase) || relative.Equals("uninstall.exe", StringComparison.OrdinalIgnoreCase)) {
							throw new InvalidOperationException("The payload contains a reserved installer file.");
						}
						string path = Within(staging, relative);
						Directory.CreateDirectory(Path.GetDirectoryName(path));
						using (Stream input = entry.Open()) {
							using (FileStream output = new FileStream(path, FileMode.CreateNew)) {
								input.CopyTo(output);
							}
						}
						manifest.Files.Add(relative, Hash(path));
					}
				}
			}
			string tool = product == "Kiln" ? "kiln.exe" : "rutilec.exe";
			string other = product == "Kiln" ? "rutilec.exe" : "kiln.exe";
			if (!manifest.Files.ContainsKey(Path.Combine("bin", tool)) || manifest.Files.ContainsKey(Path.Combine("bin", other))) {
				throw new InvalidOperationException("The payload does not contain exactly the requested independent tool.");
			}
			File.Copy(Assembly.GetExecutingAssembly().Location, Path.Combine(staging, "uninstall.exe"));
			manifest.Files.Add("uninstall.exe", Hash(Path.Combine(staging, "uninstall.exe")));
			foreach (string relative in manifest.Files.Keys) {
				string destination = Within(root, relative);
				if (!File.Exists(destination)) {
					continue;
				}
				string expected;
				string existingHash = Hash(destination);
				if (existingHash == manifest.Files[relative]) {
					continue;
				}
				if (previous == null || !previous.Files.TryGetValue(relative, out expected) || existingHash != expected) {
					throw new InvalidOperationException("Refusing to overwrite an unrelated or modified file: " + destination);
				}
			}
			Directory.CreateDirectory(root);
			List<string> copied = new List<string>();
			Dictionary<string, string> backups = new Dictionary<string, string>();
			try {
				foreach (string relative in manifest.Files.Keys) {
					string destination = Within(root, relative);
					Directory.CreateDirectory(Path.GetDirectoryName(destination));
					if (File.Exists(destination) && Hash(destination) == manifest.Files[relative]) {
						Console.WriteLine("Reused " + relative);
						continue;
					}
					if (File.Exists(destination)) {
						string backup = Path.Combine(staging, "backup-" + Guid.NewGuid().ToString("N"));
						File.Copy(destination, backup);
						backups.Add(relative, backup);
					}
					copied.Add(relative);
					File.Copy(Within(staging, relative), destination, true);
					Console.WriteLine("Installed " + relative);
				}
				manifest.AddedPath = UpdatePath(Path.Combine(root, "bin"), machine, false) || manifest.AddedPath;
				WriteManifest(root, manifest);
			}
			catch {
				foreach (string relative in copied.AsEnumerable().Reverse()) {
					string backup;
					if (backups.TryGetValue(relative, out backup)) {
						File.Copy(backup, Within(root, relative), true);
					}
					else {
						File.Delete(Within(root, relative));
					}
				}
				throw;
			}
			if (previous != null) {
				foreach (KeyValuePair<string, string> file in previous.Files) {
					if (!manifest.Files.ContainsKey(file.Key)) {
						string obsolete = Within(root, file.Key);
						if (File.Exists(obsolete) && Hash(obsolete) == file.Value) {
							File.Delete(obsolete);
						}
					}
				}
			}
			Register(root, machine);
			Console.WriteLine(product + " " + version + " installed in " + root);
			Console.WriteLine("Restart your terminal and editor to use the updated PATH.");
		}
		finally {
			Directory.Delete(staging, true);
		}
	}

	private static bool UpdatePath(string bin, bool machine, bool remove) {
		EnvironmentVariableTarget scope = machine ? EnvironmentVariableTarget.Machine : EnvironmentVariableTarget.User;
		string existing = Environment.GetEnvironmentVariable("Path", scope) ?? "";
		List<string> entries = existing.Split(';').Where(entry => !string.IsNullOrWhiteSpace(entry)).ToList();
		Func<string, bool> matches = entry => string.Equals(Environment.ExpandEnvironmentVariables(entry.Trim().Trim('"')).TrimEnd('\\', '/'), bin.TrimEnd('\\', '/'), StringComparison.OrdinalIgnoreCase);
		if (!remove && entries.Any(matches)) {
			return false;
		}
		if (remove) {
			entries.RemoveAll(entry => matches(entry));
		}
		else {
			entries.Add(bin);
		}
		Environment.SetEnvironmentVariable("Path", string.Join(";", entries), scope);
		UIntPtr result;
		SendMessageTimeout(new IntPtr(0xffff), 0x001a, UIntPtr.Zero, "Environment", 2, 5000, out result);
		return !remove;
	}

	private static string RegistryPath() {
		return @"Software\Microsoft\Windows\CurrentVersion\Uninstall\" + product;
	}

	private static void Register(string root, bool machine) {
		using (RegistryKey key = (machine ? Registry.LocalMachine : Registry.CurrentUser).CreateSubKey(RegistryPath())) {
			key.SetValue("DisplayName", product);
			key.SetValue("DisplayVersion", version);
			key.SetValue("InstallLocation", root);
			key.SetValue("UninstallString", "\"" + Path.Combine(root, "uninstall.exe") + "\" uninstall --" + (machine ? "machine" : "user") + " --prefix \"" + root + "\"");
			key.SetValue("NoModify", 1, RegistryValueKind.DWord);
			key.SetValue("NoRepair", 1, RegistryValueKind.DWord);
		}
	}

	private static void Uninstall(string root, InstallManifest manifest, bool machine) {
		if (manifest == null) {
			throw new InvalidOperationException("No " + product + " installation manifest exists at " + root);
		}
		string running = Assembly.GetExecutingAssembly().Location;
		foreach (KeyValuePair<string, string> file in manifest.Files) {
			string path = Within(root, file.Key);
			if (!File.Exists(path)) {
				continue;
			}
			if (Hash(path) != file.Value) {
				Console.WriteLine("Preserved modified file: " + path);
				continue;
			}
			if (string.Equals(path, running, StringComparison.OrdinalIgnoreCase)) {
				Console.WriteLine("Delete this uninstaller after it closes: " + path);
				continue;
			}
			File.Delete(path);
			Console.WriteLine("Removed " + file.Key);
		}
		if (manifest.AddedPath) {
			UpdatePath(Path.Combine(root, "bin"), machine, true);
		}
		RegistryKey hive = machine ? Registry.LocalMachine : Registry.CurrentUser;
		using (RegistryKey key = hive.OpenSubKey(RegistryPath())) {
			if (key != null && string.Equals(key.GetValue("InstallLocation") as string, root, StringComparison.OrdinalIgnoreCase)) {
				key.Close();
				hive.DeleteSubKeyTree(RegistryPath(), false);
			}
		}
		File.Delete(Path.Combine(root, ManifestName));
		Console.WriteLine(product + " uninstalled. Unlisted and modified files were preserved.");
	}
}
