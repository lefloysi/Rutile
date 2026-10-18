using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio.LanguageServer.Client;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;

namespace Rutile.VisualStudio {
	internal sealed class ServerProcess : IDisposable {
		private readonly Process process;
		private readonly Func<Task> onExited;
		private CancellationTokenRegistration cancellation;
		private int disposed;
		private Stream? input;
		private Stream? output;
		private StreamReader? errors;

		public Connection Connection { get; }

		public ServerProcess(string executable, string folder, CancellationToken token, Func<Task> onExited) {
			if (!File.Exists(executable)) {
				throw new FileNotFoundException("The Rutile language server is missing from the extension.", executable);
			}

			this.onExited = onExited;
			process = new Process {
				StartInfo = new ProcessStartInfo(executable) {
					UseShellExecute = false,
					RedirectStandardInput = true,
					RedirectStandardOutput = true,
					RedirectStandardError = true,
					CreateNoWindow = true,
					WorkingDirectory = folder,
					Arguments = "language-server --stdio"
				},
				EnableRaisingEvents = true
			};
			process.Exited += OnExited;

			try {
				if (!process.Start()) {
					throw new IOException("The Rutile language server could not be started.");
				}

				input = process.StandardInput.BaseStream;
				output = process.StandardOutput.BaseStream;
				errors = process.StandardError;
				Connection = new Connection(new ConnectionStream(output, this), new ConnectionStream(input, this));
				_ = ReadErrorsAsync();
				cancellation = token.Register(Dispose);
			}
			catch {
				Dispose();
				throw;
			}
		}

		private async Task ReadErrorsAsync() {
			try {
				var reader = errors;
				if (reader == null) {
					return;
				}

				string? line;
				while ((line = await reader.ReadLineAsync().ConfigureAwait(false)) != null) {
					ServerOutput.Write(line);
				}
			}
			catch (ObjectDisposedException) {
			}
			catch (IOException error) {
				if (Volatile.Read(ref disposed) == 0) {
					ServerOutput.Write(error.Message);
				}
			}
		}

		private void OnExited(object sender, EventArgs arguments) {
			if (Volatile.Read(ref disposed) != 0) {
				return;
			}

			ServerOutput.Write("Rutile language server exited with code " + process.ExitCode + ".");
			_ = ThreadHelper.JoinableTaskFactory.RunAsync(async () => {
				try {
					await onExited().ConfigureAwait(false);
				}
				catch (Exception error) {
					ServerOutput.Write(error.Message);
				}
			});
		}

		public void Dispose() {
			if (Interlocked.Exchange(ref disposed, 1) != 0) {
				return;
			}

			cancellation.Dispose();
			process.Exited -= OnExited;
			try {
				if (!process.HasExited) {
					process.Kill();
				}
			}
			catch (InvalidOperationException) {
			}
			catch (System.ComponentModel.Win32Exception error) {
				ServerOutput.Write(error.Message);
			}
			finally {
				input?.Dispose();
				output?.Dispose();
				errors?.Dispose();
				process.Dispose();
			}
		}
	}

	internal sealed class ConnectionStream : Stream {
		private readonly Stream stream;
		private readonly ServerProcess owner;

		public ConnectionStream(Stream stream, ServerProcess owner) {
			this.stream = stream;
			this.owner = owner;
		}

		public override bool CanRead => stream.CanRead;
		public override bool CanWrite => stream.CanWrite;
		public override bool CanSeek => false;
		public override long Length => throw new NotSupportedException();
		public override long Position {
			get => throw new NotSupportedException();
			set => throw new NotSupportedException();
		}

		public override void Flush() {
			stream.Flush();
		}

		public override Task FlushAsync(CancellationToken cancellationToken) {
			return stream.FlushAsync(cancellationToken);
		}

		public override int Read(byte[] buffer, int offset, int count) {
			return stream.Read(buffer, offset, count);
		}

		public override Task<int> ReadAsync(byte[] buffer, int offset, int count, CancellationToken cancellationToken) {
			return stream.ReadAsync(buffer, offset, count, cancellationToken);
		}

		public override void Write(byte[] buffer, int offset, int count) {
			stream.Write(buffer, offset, count);
		}

		public override Task WriteAsync(byte[] buffer, int offset, int count, CancellationToken cancellationToken) {
			return stream.WriteAsync(buffer, offset, count, cancellationToken);
		}

		public override long Seek(long offset, SeekOrigin origin) {
			throw new NotSupportedException();
		}

		public override void SetLength(long value) {
			throw new NotSupportedException();
		}

		protected override void Dispose(bool disposing) {
			if (disposing) {
				owner.Dispose();
			}
			base.Dispose(disposing);
		}
	}

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
