using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Threading;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using Newtonsoft.Json.Linq;

namespace Rutile.VisualStudio {
	internal sealed class KilnWorkspaceControl : UserControl {
		private readonly ComboBox profiles = new ComboBox { MinWidth = 120, Margin = new Thickness(4) };
		private readonly ComboBox targets = new ComboBox { MinWidth = 160, Margin = new Thickness(4) };
		private readonly TextBox arguments = new TextBox { MinWidth = 160, Margin = new Thickness(4), ToolTip = "Program arguments" };
		private readonly TextBlock status = new TextBlock { Text = "Open a folder containing kiln.rt.", TextWrapping = TextWrapping.Wrap, Margin = new Thickness(8) };
		private readonly StackPanel actions = new StackPanel { Orientation = Orientation.Horizontal };
		private WorkspaceRootMonitor? roots;
		private KilnProject? project;
		private FileSystemWatcher? watcher;
		private JObject? description;
		private int refreshVersion;
		private readonly KilnDiagnostics diagnostics = new KilnDiagnostics();
		private readonly DispatcherTimer statusTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(2) };
		private Task? statusRequest;
		private string? lastBuildState;
		public bool HasProject => description != null && project != null && File.Exists(Path.Combine(project.Root, "kiln.rt"));
		public bool IsBusy => !actions.IsEnabled;
		public string SelectedProfile => profiles.SelectedItem as string ?? "";
		public string SelectedTarget => targets.SelectedItem as string ?? "";
		public string[] Profiles => profiles.Items.Cast<string>().ToArray();
		public string[] Targets => targets.Items.Cast<string>().ToArray();

		public void SelectProfile(string profile) {
			profiles.SelectedItem = profile;
		}

		public void SelectTarget(string target) {
			targets.SelectedItem = target;
		}

		public KilnWorkspaceControl() {
			var panel = new StackPanel();
			panel.Children.Add(new TextBlock { Text = "Profile", Margin = new Thickness(8, 8, 8, 0) });
			panel.Children.Add(profiles);
			panel.Children.Add(new TextBlock { Text = "Target", Margin = new Thickness(8, 8, 8, 0) });
			panel.Children.Add(targets);
			panel.Children.Add(new TextBlock { Text = "Program arguments", Margin = new Thickness(8, 8, 8, 0) });
			panel.Children.Add(arguments);
			AddButton("Refresh", RefreshAsync);
			AddButton("Build", () => BuildAsync("build"));
			AddButton("Run", () => BuildAsync("run"));
			AddButton("Debug", () => BuildAsync("debug"));
			panel.Children.Add(actions);
			panel.Children.Add(status);
			Content = panel;
			statusTimer.Tick += (sender, change) => {
				if (statusRequest == null || statusRequest.IsCompleted) {
					statusRequest = ExecuteAsync(PollStatusAsync);
				}
			};
			profiles.SelectionChanged += (sender, change) => {
				if (description != null) {
					_ = ExecuteAsync(RefreshAsync);
				}
			};
			targets.SelectionChanged += (sender, change) => SaveSelection();
		}

		private void AddButton(string text, Func<Task> action) {
			var button = new Button { Content = text, Margin = new Thickness(4), Padding = new Thickness(8, 3, 8, 3) };
			button.Click += (sender, change) => { _ = ThreadHelper.JoinableTaskFactory.RunAsync(() => ExecuteAsync(action)); };
			actions.Children.Add(button);
		}

		private async Task ExecuteAsync(Func<Task> action) {
			try {
				await action();
			}
			catch (Exception error) {
				status.Text = error.Message;
				KilnOutput.Write(error.ToString(), true);
			}
		}

		public async Task InitializeAsync() {
			await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
			if (roots == null) {
				roots = new WorkspaceRootMonitor(() => ExecuteAsync(RefreshAsync));
			}
			await ExecuteAsync(RefreshAsync);
			statusTimer.Start();
		}

		public void Invoke(string operation) {
			_ = ThreadHelper.JoinableTaskFactory.RunAsync(() => ExecuteAsync(() => BuildAsync(operation)));
		}

		public void CloseWorkspace() {
			ThreadHelper.ThrowIfNotOnUIThread();
			project = null;
			description = null;
			refreshVersion++;
			statusTimer.Stop();
			watcher?.Dispose();
			watcher = null;
			roots?.Dispose();
			roots = null;
			diagnostics.Dispose();
		}

		private async Task PollStatusAsync() {
			await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
			var current = project;
			var profile = profiles.SelectedItem as string;
			var target = targets.SelectedItem as string;
			if (current == null || profile == null || target == null || !HasProject || IsBusy) {
				return;
			}
			var response = await current.RequestAsync("describe", profile);
			await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
			if (current != project || IsBusy || profile != profiles.SelectedItem as string || target != targets.SelectedItem as string) {
				return;
			}
			var build = (response["builds"] as JArray)?.FirstOrDefault(item => item.Value<string>("target") == target && item.Value<string>("profile") == profile);
			if (build == null) {
				return;
			}
			var state = build.ToString(Newtonsoft.Json.Formatting.None);
			if (state == lastBuildState) {
				return;
			}
			lastBuildState = state;
			var messages = build.Value<string>("diagnostics") ?? "";
			diagnostics.Update(current.Root, messages);
			if (build.Value<bool?>("succeeded") == false) {
				status.Text = "Background build failed for " + target + ". " + messages;
				ServerOutput.Write(status.Text);
				return;
			}
			status.Text = build.Value<bool?>("up_to_date") == true ? target + " is up to date (" + profile + ")." : "Built " + target + " (" + profile + "). Kiln is watching for changes.";
			KilnOutput.Write(status.Text);
		}

		private async Task RefreshAsync() {
			await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
			var version = ++refreshVersion;
			var root = roots?.CurrentRoot();
			if (root == null) {
				project = null;
				description = null;
				profiles.Items.Clear();
				targets.Items.Clear();
				status.Text = "Open a folder containing kiln.rt.";
				return;
			}
			if (!File.Exists(Path.Combine(root, "kiln.rt"))) {
				description = null;
				status.Text = "This folder has no kiln.rt project configuration.";
				return;
			}
			if (project == null || !string.Equals(project.Root, root, StringComparison.OrdinalIgnoreCase)) {
				project = new KilnProject(Path.Combine(Path.GetDirectoryName(typeof(KilnProject).Assembly.Location)!, "kiln.exe"), root, message => KilnOutput.Write(message));
				KilnOutput.Write("Loading Kiln project: " + root, true);
				watcher?.Dispose();
				watcher = new FileSystemWatcher(root, "kiln.rt") { IncludeSubdirectories = true, NotifyFilter = NotifyFilters.LastWrite | NotifyFilters.FileName, EnableRaisingEvents = true };
				watcher.Changed += OnConfigurationChanged;
				watcher.Created += OnConfigurationChanged;
				watcher.Deleted += OnConfigurationChanged;
				watcher.Renamed += OnConfigurationChanged;
				description = null;
				profiles.Items.Clear();
				var savedProfile = ReadSelection().Value<string>("profile") ?? "debug";
				profiles.Items.Add(savedProfile);
				profiles.SelectedItem = savedProfile;
			}
			var current = project;
			var profile = profiles.SelectedItem as string ?? "debug";
			lastBuildState = null;
			status.Text = "Loading Kiln targets...";
			var response = await current.RequestAsync("describe", profile);
			await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
			if (version != refreshVersion || project != current) {
				return;
			}
			var selected = targets.SelectedItem as string ?? ReadSelection().Value<string>("target");
			description = null;
			profiles.Items.Clear();
			foreach (var item in response["profiles"] ?? new JArray()) {
				profiles.Items.Add(item.Value<string>("name"));
			}
			profiles.SelectedItem = profile;
			if (profiles.SelectedIndex < 0 && profiles.Items.Count > 0) {
				profiles.SelectedIndex = 0;
			}
			targets.Items.Clear();
			foreach (var item in response["targets"] ?? new JArray()) {
				targets.Items.Add(item.Value<string>("name"));
			}
			targets.SelectedItem = selected;
			if (targets.SelectedIndex < 0 && targets.Items.Count > 0) {
				var executable = (response["targets"] as JArray)?.FirstOrDefault(item => !string.IsNullOrEmpty(item.Value<string>("executable")));
				targets.SelectedItem = executable?.Value<string>("name") ?? targets.Items[0];
			}
			description = response;
			if (profiles.SelectedItem is string selectedProfile && selectedProfile != profile) {
				await RefreshAsync();
				return;
			}
			KilnWorkspaceFiles.Update(current.Root, Path.Combine(Path.GetDirectoryName(typeof(KilnProject).Assembly.Location)!, "kiln.exe"), profile, response);
			SaveSelection();
			status.Text = targets.Items.Count + " targets. Kiln keeps the project session running and rebuilds watched targets.";
			KilnOutput.Write("Kiln project session ready. Profiles: " + string.Join(", ", Profiles) + ".");
			foreach (var item in response["targets"] ?? new JArray()) {
				KilnOutput.Write("  " + item.Value<string>("name") + " [" + item.Value<string>("compiler") + ", " + item.Value<string>("type") + "]");
			}
		}

		private void OnConfigurationChanged(object sender, FileSystemEventArgs change) {
			if (Path.GetFileName(change.FullPath) != "kiln.rt") {
				return;
			}
			_ = ThreadHelper.JoinableTaskFactory.RunAsync(async () => {
				await Task.Delay(250);
				await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
				if (roots != null) {
					await ExecuteAsync(RefreshAsync);
				}
			});
		}

		private async Task BuildAsync(string operation) {
			await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
			var current = project;
			var target = targets.SelectedItem as string;
			var profile = profiles.SelectedItem as string;
			if (current == null || target == null || profile == null) {
				throw new InvalidOperationException("Select a Kiln target and profile first.");
			}
			var programArguments = arguments.Text;
			actions.IsEnabled = false;
			status.Text = "Building " + target + " (" + profile + ")...";
			KilnOutput.Write(status.Text, true);
			diagnostics.Update(current.Root, "");
			try {
				var result = await current.RequestAsync("build", profile, target);
				await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
				var artifact = result.Value<string>("artifact");
				status.Text = "Built " + artifact;
				KilnOutput.Write(result.Value<string>("diagnostics") ?? "");
				KilnOutput.Write(status.Text);
				if (operation == "build") {
					return;
				}
				var executable = result.Value<string>("executable");
				if (string.IsNullOrEmpty(executable)) {
					throw new InvalidOperationException("The selected target is a library. Select an executable to run or debug.");
				}
				if (operation == "run") {
					Process.Start(new ProcessStartInfo(executable, programArguments) { WorkingDirectory = current.Root, UseShellExecute = true });
					return;
				}
				LaunchDebugger(executable!, current.Root, programArguments);
			}
			catch (KilnRequestException error) {
				await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
				diagnostics.Update(current.Root, error.Message);
				throw;
			}
			finally {
				actions.IsEnabled = true;
			}
		}

		private static void LaunchDebugger(string executable, string root, string arguments) {
			ThreadHelper.ThrowIfNotOnUIThread();
			var target = new VsDebugTargetInfo {
				cbSize = (uint)Marshal.SizeOf(typeof(VsDebugTargetInfo)),
				dlo = DEBUG_LAUNCH_OPERATION.DLO_CreateProcess,
				bstrExe = executable,
				bstrArg = arguments,
				bstrCurDir = root,
				clsidCustom = new Guid("3b476d35-a401-11d2-aad4-00c04f990171")
			};
			var memory = Marshal.AllocCoTaskMem(Marshal.SizeOf(target));
			try {
				Marshal.StructureToPtr(target, memory, false);
				var debugger = Package.GetGlobalService(typeof(SVsShellDebugger)) as IVsDebugger ?? throw new InvalidOperationException("Visual Studio's native debugger is unavailable.");
				ErrorHandler.ThrowOnFailure(debugger.LaunchDebugTargets(1, memory));
			}
			finally {
				Marshal.DestroyStructure(memory, typeof(VsDebugTargetInfo));
				Marshal.FreeCoTaskMem(memory);
			}
		}

		private JObject ReadSelection() {
			if (project == null) {
				return new JObject();
			}
			var path = Path.Combine(project.Root, ".vs", "kiln", "selection.json");
			try {
				return File.Exists(path) ? JObject.Parse(File.ReadAllText(path)) : new JObject();
			}
			catch (Exception error) when (error is IOException || error is Newtonsoft.Json.JsonException) {
				return new JObject();
			}
		}

		private void SaveSelection() {
			if (description == null || project == null) {
				return;
			}
			try {
				var folder = Path.Combine(project.Root, ".vs", "kiln");
				Directory.CreateDirectory(folder);
				File.WriteAllText(Path.Combine(folder, "selection.json"), new JObject { ["profile"] = profiles.SelectedItem as string, ["target"] = targets.SelectedItem as string }.ToString());
			}
			catch (Exception error) when (error is IOException || error is UnauthorizedAccessException) {
				ServerOutput.Write(error.Message);
			}
		}
	}
}
