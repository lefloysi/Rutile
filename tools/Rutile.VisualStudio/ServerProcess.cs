using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio.LanguageServer.Client;

namespace Rutile.VisualStudio {
	internal sealed class ServerProcess : IDisposable {
		private readonly Process process;
		private readonly Func<ServerProcess, Task> onExited;
		private readonly Action<string> log;
		private readonly object processGate = new object();
		private CancellationTokenRegistration cancellation;
		private int disposed;
		private Stream? input;
		private Stream? output;
		private StreamReader? errors;

		public Connection Connection { get; }

		public ServerProcess(string executable, string folder, CancellationToken token, Func<ServerProcess, Task> onExited, Action<string> log) {
			if (!File.Exists(executable)) {
				throw new FileNotFoundException("The Rutile language server is missing from the extension.", executable);
			}

			this.onExited = onExited;
			this.log = log;
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
				if (Volatile.Read(ref disposed) != 0) {
					cancellation.Dispose();
				}
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
					log(line);
				}
			}
			catch (ObjectDisposedException) {
			}
			catch (IOException error) {
				if (Volatile.Read(ref disposed) == 0) {
					log(error.Message);
				}
			}
		}

		private void OnExited(object sender, EventArgs arguments) {
			lock (processGate) {
				if (Volatile.Read(ref disposed) != 0) {
					return;
				}
				log("Rutile language server exited with code " + process.ExitCode + ".");
			}

			_ = NotifyExitedAsync();
		}

		private async Task NotifyExitedAsync() {
			try {
				await onExited(this).ConfigureAwait(false);
			}
			catch (Exception error) {
				log(error.ToString());
			}
		}

		public void Dispose() {
			if (Interlocked.Exchange(ref disposed, 1) != 0) {
				return;
			}

			cancellation.Dispose();
			lock (processGate) {
				process.Exited -= OnExited;
				try {
					if (!process.HasExited) {
						process.Kill();
					}
				}
				catch (InvalidOperationException) {
				}
				catch (System.ComponentModel.Win32Exception error) {
					log(error.Message);
				}
				finally {
					input?.Dispose();
					output?.Dispose();
					errors?.Dispose();
					process.Dispose();
				}
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

}
