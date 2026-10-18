using System;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Rutile.VisualStudio;

internal static class ProcessTests {
	private static void Require(bool condition, string reason) {
		if (!condition) {
			throw new InvalidOperationException(reason);
		}
	}

	private static async Task Until(Func<bool> condition) {
		var timeout = Stopwatch.StartNew();
		while (!condition()) {
			if (timeout.Elapsed > TimeSpan.FromSeconds(10)) {
				throw new TimeoutException("The child process did not reach the expected state.");
			}
			await Task.Delay(20);
		}
	}

	private static bool Exited(int id) {
		try {
			using (var child = Process.GetProcessById(id)) {
				return child.HasExited;
			}
		}
		catch (ArgumentException) {
			return true;
		}
	}

	private static async Task<int> Child() {
		using var ownership = new FileStream("process-owner.lock", FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None);
		Console.OutputEncoding = new UTF8Encoding(false);
		Console.WriteLine(Process.GetCurrentProcess().Id + "|" + Environment.CurrentDirectory);
		for (var index = 0; index < 8192; ++index) {
			Console.Error.WriteLine("stderr " + index + " " + new string('x', 100));
		}
		Console.Error.WriteLine("stderr-drained");
		string? line;
		while ((line = await Console.In.ReadLineAsync()) != null) {
			if (line == "exit") {
				return 7;
			}
			Console.WriteLine("pong");
		}
		return 0;
	}

	private static async Task Test(string directory) {
		var executable = typeof(ProcessTests).Assembly.Location;
		var logs = new ConcurrentQueue<string>();
		var notified = new TaskCompletionSource<ServerProcess>(TaskCreationOptions.RunContinuationsAsynchronously);
		using (var server = new ServerProcess(executable, directory, CancellationToken.None, source => {
			notified.TrySetResult(source);
			return Task.CompletedTask;
		}, logs.Enqueue)) {
			var reader = new StreamReader(server.Connection.Input, Encoding.UTF8, true, 1024, true);
			var writer = new StreamWriter(server.Connection.Output, new UTF8Encoding(false), 1024, true) { AutoFlush = true };
			var startup = (await reader.ReadLineAsync())!.Split('|');
			var id = int.Parse(startup[0]);
			Require(Path.GetFullPath(startup[1]) == Path.GetFullPath(directory), "Server working directory was lost.");
			await writer.WriteLineAsync("ping");
			Require(await reader.ReadLineAsync() == "pong", "Stderr drainage blocked protocol output.");
			await Until(() => logs.Contains("stderr-drained"));
			server.Connection.Output.Dispose();
			await Until(() => Exited(id));
			Require(!notified.Task.IsCompleted, "Disposal was reported as an unexpected exit.");
		}
		Console.WriteLine("connection disposal, working directory and stderr drainage: PASS");

		using (var cancellation = new CancellationTokenSource()) {
			using (var server = new ServerProcess(executable, directory, cancellation.Token, _ => Task.CompletedTask, logs.Enqueue)) {
				var reader = new StreamReader(server.Connection.Input);
				var id = int.Parse((await reader.ReadLineAsync())!.Split('|')[0]);
				await Task.WhenAll(Enumerable.Range(0, 8).Select(_ => Task.Run(() => server.Dispose())).Concat(new[] { Task.Run(() => cancellation.Cancel()) }));
				await Until(() => Exited(id));
			}
		}
		Console.WriteLine("concurrent disposal and cancellation: PASS");

		logs = new ConcurrentQueue<string>();
		notified = new TaskCompletionSource<ServerProcess>(TaskCreationOptions.RunContinuationsAsynchronously);
		using (var server = new ServerProcess(executable, directory, CancellationToken.None, source => {
			notified.TrySetResult(source);
			return Task.CompletedTask;
		}, logs.Enqueue)) {
			var reader = new StreamReader(server.Connection.Input);
			await reader.ReadLineAsync();
			var writer = new StreamWriter(server.Connection.Output) { AutoFlush = true };
			await writer.WriteLineAsync("exit");
			await Until(() => notified.Task.IsCompleted);
			Require(await notified.Task == server, "Exit callback lost the owning process identity.");
			Require(logs.Any(line => line.Contains("exited with code 7")), "Exit failure was not reported.");
		}
		Console.WriteLine("unexpected exit identity and diagnostics: PASS");

		ServerProcess? current = null;
		var owners = new System.Collections.Generic.List<int>();
		try {
			for (var index = 0; index < 25; ++index) {
				current?.Dispose();
				current = new ServerProcess(executable, directory, CancellationToken.None, _ => Task.CompletedTask, _ => { });
				var reader = new StreamReader(current.Connection.Input);
				var startup = await reader.ReadLineAsync();
				Require(startup != null, "Immediate replacement could not acquire the previous owner's exclusive lock.");
				owners.Add(int.Parse(startup!.Split('|')[0]));
			}
		}
		finally {
			current?.Dispose();
		}
		await Until(() => owners.All(Exited));
		Console.WriteLine("25 immediate owner replacements with an exclusive project lock: PASS");
	}

	public static int Main(string[] arguments) {
		if (arguments.Length != 0 && arguments[0] == "language-server") {
			return Child().GetAwaiter().GetResult();
		}
		try {
			var directory = Path.Combine(Path.GetDirectoryName(typeof(ProcessTests).Assembly.Location)!, "project folder");
			Directory.CreateDirectory(directory);
			Test(directory).GetAwaiter().GetResult();
			return 0;
		}
		catch (Exception error) {
			Console.Error.WriteLine(error);
			return 1;
		}
	}
}
