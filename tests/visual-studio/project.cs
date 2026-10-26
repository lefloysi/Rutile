using System;
using System.IO;
using System.IO.Pipes;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Editor;
using Newtonsoft.Json;
using Rutile.VisualStudio;

internal sealed class EditorDocumentFocusFixture {
	internal readonly ITextBuffer Buffer;
	internal readonly ITextDocument Document;
	internal readonly IWpfTextView View;
	internal readonly ITextDocumentFactoryService Factory;
	internal bool Available;
	internal int Subscriptions;
	private EventHandler<TextDocumentEventArgs>? created;
	private EventHandler? closed;

	internal EditorDocumentFocusFixture(string path, bool available) {
		Available = available;
		Buffer = new EditorInterface<ITextBuffer>((method, values) => null).Value;
		Document = new EditorInterface<ITextDocument>((method, values) => method.Name == "get_TextBuffer" ? (object)Buffer : method.Name == "get_FilePath" ? path : null).Value;
		View = new EditorInterface<IWpfTextView>((method, values) => {
			if (method.Name == "get_TextBuffer") {
				return Buffer;
			}
			if (method.Name == "get_HasAggregateFocus") {
				return false;
			}
			if (method.Name == "add_Closed") {
				closed += (EventHandler)values[0];
			}
			return null;
		}).Value;
		Factory = new EditorInterface<ITextDocumentFactoryService>((method, values) => {
			if (method.Name == "TryGetTextDocument") {
				values[1] = Available ? Document : null!;
				return Available;
			}
			if (method.Name == "add_TextDocumentCreated") {
				created += (EventHandler<TextDocumentEventArgs>)values[0];
				Subscriptions = created.GetInvocationList().Length;
			}
			if (method.Name == "remove_TextDocumentCreated") {
				created -= (EventHandler<TextDocumentEventArgs>)values[0];
				Subscriptions = created?.GetInvocationList().Length ?? 0;
			}
			return null;
		}).Value;
	}

	internal void Created(ITextDocument document) {
		created?.Invoke(this, new TextDocumentEventArgs(document));
	}

	internal void Close() {
		closed?.Invoke(this, EventArgs.Empty);
	}
}

public static partial class EditorObservations {
	public static string DocumentFocus() {
		var previous = RutileDocumentProject.ActivePath;
		if (previous != null) {
			RutileDocumentProject.Close(previous);
		}
		var path = Path.Combine(Path.GetTempPath(), "rutile-first-document.rt");
		var first = new EditorDocumentFocusFixture(path, true);
		new RutileDocumentFocus { Documents = first.Factory }.TextViewCreated(first.View);
		var initialized = RutileDocumentProject.ActivePath == path && first.Subscriptions == 0;
		var background = new EditorDocumentFocusFixture(path + ".background", true);
		new RutileDocumentFocus { Documents = background.Factory }.TextViewCreated(background.View);
		var preserved = RutileDocumentProject.ActivePath == path;
		background.Close();
		first.Available = false;
		first.Close();
		var closed = RutileDocumentProject.ActivePath == null;

		var late = new EditorDocumentFocusFixture(path, false);
		new RutileDocumentFocus { Documents = late.Factory }.TextViewCreated(late.View);
		var waiting = late.Subscriptions == 1 && RutileDocumentProject.ActivePath == null;
		late.Created(background.Document);
		var unrelated = RutileDocumentProject.ActivePath == null && late.Subscriptions == 1;
		late.Available = true;
		late.Created(late.Document);
		var attached = RutileDocumentProject.ActivePath == path && late.Subscriptions == 0;
		late.Close();

		var abandoned = new EditorDocumentFocusFixture(path, false);
		new RutileDocumentFocus { Documents = abandoned.Factory }.TextViewCreated(abandoned.View);
		abandoned.Close();
		abandoned.Created(abandoned.Document);
		var cleaned = abandoned.Subscriptions == 0 && RutileDocumentProject.ActivePath == null;
		if (previous != null) {
			RutileDocumentProject.Activate(previous);
		}
		return JsonConvert.SerializeObject(new { initialized, preserved, closed, waiting, unrelated, attached, cleaned });
	}

	public static string ProjectEnvironment() {
		return ObserveProjectEnvironmentAsync().GetAwaiter().GetResult();
	}

	private static async Task<string> ObserveProjectEnvironmentAsync() {
		var root = Path.Combine(Path.GetTempPath(), "rutile-editor-environment-" + Guid.NewGuid().ToString("N"));
		Directory.CreateDirectory(root);
		NamedPipeServerStream? server = null;
		try {
			var endpoint = (string)typeof(RutileServiceConnection).GetMethod("Endpoint", BindingFlags.NonPublic | BindingFlags.Static)!.Invoke(null, new object[] { root })!;
			var pipeName = endpoint.Substring(@"\\.\pipe\".Length);
			var configurations = 0;
			Func<string, CancellationToken, Action<string>, Task> configure = (directory, token, log) => {
				++configurations;
				server = new NamedPipeServerStream(pipeName, PipeDirection.InOut, 1, PipeTransmissionMode.Byte, PipeOptions.Asynchronous);
				return Task.CompletedTask;
			};
			using (var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(10))) {
				using (var connection = await RutileServiceConnection.ConnectAsync(root, timeout.Token, value => Task.CompletedTask, value => { }, configure).ConfigureAwait(false)) {
					await server!.WaitForConnectionAsync(timeout.Token).ConfigureAwait(false);
				}
				server!.Dispose();
				server = new NamedPipeServerStream(pipeName, PipeDirection.InOut, 1, PipeTransmissionMode.Byte, PipeOptions.Asynchronous);
				using (var connection = await RutileServiceConnection.ConnectAsync(root, timeout.Token, value => Task.CompletedTask, value => { }, configure).ConfigureAwait(false)) {
					await server.WaitForConnectionAsync(timeout.Token).ConfigureAwait(false);
				}
				return JsonConvert.SerializeObject(new { configurations });
			}
		}
		finally {
			server?.Dispose();
			Directory.Delete(root, true);
		}
	}

	public static string DocumentProject() {
		var root = Path.Combine(Path.GetTempPath(), "rutile-editor-roots-" + Guid.NewGuid().ToString("N"));
		var nested = Path.Combine(root, "nested");
		Directory.CreateDirectory(Path.Combine(nested, "source"));
		try {
			File.WriteAllText(Path.Combine(root, "kiln.rt"), "");
			File.WriteAllText(Path.Combine(nested, "kiln.rt"), "");
			var outer = RutileDocumentProject.FindRoot(Path.Combine(root, "source.rt"));
			var inner = RutileDocumentProject.FindRoot(Path.Combine(nested, "source", "source.rt"));
			var build = RutileDocumentProject.FindRoot(Path.Combine(nested, "kiln.rt"));
			var unrelated = RutileDocumentProject.FindRoot(Path.Combine(Path.GetTempPath(), "unowned.rt"));
			var changes = 0;
			EventHandler changed = (_, __) => ++changes;
			RutileDocumentProject.Changed += changed;
			try {
				RutileDocumentProject.Activate(Path.Combine(root, "source.rt"));
				RutileDocumentProject.Activate(Path.Combine(root, "source.rt"));
				RutileDocumentProject.Activate(Path.Combine(nested, "source", "source.rt"));
				RutileDocumentProject.Close(Path.Combine(root, "source.rt"));
				RutileDocumentProject.Close(Path.Combine(nested, "source", "source.rt"));
			}
			finally {
				RutileDocumentProject.Changed -= changed;
			}
			return JsonConvert.SerializeObject(new { outer = outer == root, inner = inner == nested, build = build == nested, unrelated = unrelated == null, empty = RutileDocumentProject.FindRoot(null) == null, changes });
		}
		finally {
			Directory.Delete(root, true);
		}
	}
}
