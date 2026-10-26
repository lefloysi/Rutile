using System;
using System.IO;
using System.IO.Pipes;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio.LanguageServer.Client;

namespace Rutile.VisualStudio {
	internal sealed class RutileServiceConnection : IDisposable {
		private readonly NamedPipeClientStream pipe;
		private readonly Func<RutileServiceConnection, Task> onDisconnected;
		private readonly Action<string> log;
		private CancellationTokenRegistration cancellation;
		private int disposed;
		private int notified;

		public Connection Connection { get; }
		public string Root { get; }
		internal bool IsDisposed => Volatile.Read(ref disposed) != 0 || Volatile.Read(ref notified) != 0;

		private RutileServiceConnection(string root, NamedPipeClientStream pipe, CancellationToken token, Func<RutileServiceConnection, Task> onDisconnected, Action<string> log) {
			Root = Path.GetFullPath(root);
			this.pipe = pipe;
			this.onDisconnected = onDisconnected;
			this.log = log;
			Connection = new Connection(new ServiceConnectionStream(pipe, this), new ServiceConnectionStream(pipe, this));
			cancellation = token.Register(Dispose);
			if (Volatile.Read(ref disposed) != 0) {
				cancellation.Dispose();
			}
		}

		public static async Task<RutileServiceConnection> ConnectAsync(string root, CancellationToken token, Func<RutileServiceConnection, Task> onDisconnected, Action<string> log) {
			return await ConnectAsync(root, token, onDisconnected, log, RutileProjectEnvironment.ConfigureAsync).ConfigureAwait(false);
		}

		internal static async Task<RutileServiceConnection> ConnectAsync(string root, CancellationToken token, Func<RutileServiceConnection, Task> onDisconnected, Action<string> log, Func<string, CancellationToken, Action<string>, Task> configure) {
			var endpoint = Endpoint(root);
			var existing = await TryConnectAsync(root, endpoint, 250, token, onDisconnected, log).ConfigureAwait(false);
			if (existing != null) {
				return existing;
			}
			await configure(root, token, log).ConfigureAwait(false);
			var deadline = DateTime.UtcNow.AddSeconds(30);
			while (DateTime.UtcNow < deadline) {
				token.ThrowIfCancellationRequested();
				var connection = await TryConnectAsync(root, endpoint, 1000, token, onDisconnected, log).ConfigureAwait(false);
				if (connection != null) {
					return connection;
				}
			}
			throw new TimeoutException("Rutilec did not accept the Visual Studio language connection within 30 seconds after project configuration.");
		}

		private static async Task<RutileServiceConnection?> TryConnectAsync(string root, string endpoint, int timeout, CancellationToken token, Func<RutileServiceConnection, Task> onDisconnected, Action<string> log) {
			token.ThrowIfCancellationRequested();
			const string prefix = @"\\.\pipe\";
			if (!endpoint.StartsWith(prefix, StringComparison.OrdinalIgnoreCase) || endpoint.Length == prefix.Length) {
				throw new IOException("Rutilec published an invalid Visual Studio language connection endpoint.");
			}

			var pipe = new NamedPipeClientStream(".", endpoint.Substring(prefix.Length), PipeDirection.InOut, PipeOptions.Asynchronous);
			try {
				await pipe.ConnectAsync(timeout, token).ConfigureAwait(false);
				return new RutileServiceConnection(root, pipe, token, onDisconnected, log);
			}
			catch (TimeoutException) {
				pipe.Dispose();
			}
			catch (IOException) {
				pipe.Dispose();
			}
			catch {
				pipe.Dispose();
				throw;
			}
			return null;
		}

		private static string Endpoint(string root) {
			using (var directory = CreateFile(Path.GetFullPath(root), 0x80, 7, IntPtr.Zero, 3, 0x02000000, IntPtr.Zero)) {
				if (directory.IsInvalid || !GetFileInformationByHandle(directory, out var identity)) {
					throw new IOException("Cannot identify the Rutile project directory.", Marshal.GetExceptionForHR(Marshal.GetHRForLastWin32Error()));
				}
				return @"\\.\pipe\kiln-" + identity.Volume.ToString("x8") + "-" + identity.IndexHigh.ToString("x8") + identity.IndexLow.ToString("x8") + "-rutilec-lsp";
			}
		}

		[StructLayout(LayoutKind.Sequential)]
		private struct FileIdentity {
			public uint Attributes;
			public System.Runtime.InteropServices.ComTypes.FILETIME Creation;
			public System.Runtime.InteropServices.ComTypes.FILETIME Access;
			public System.Runtime.InteropServices.ComTypes.FILETIME Write;
			public uint Volume;
			public uint SizeHigh;
			public uint SizeLow;
			public uint Links;
			public uint IndexHigh;
			public uint IndexLow;
		}

		[DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
		private static extern SafeFileHandle CreateFile(string path, uint access, uint share, IntPtr security, uint disposition, uint flags, IntPtr template);
		[DllImport("kernel32.dll", SetLastError = true)]
		private static extern bool GetFileInformationByHandle(SafeFileHandle file, out FileIdentity identity);

		private void Disconnected() {
			if (Volatile.Read(ref disposed) != 0 || Interlocked.Exchange(ref notified, 1) != 0) {
				return;
			}
			log("The persistent Rutilec language connection closed.");
			_ = NotifyDisconnectedAsync();
		}

		private async Task NotifyDisconnectedAsync() {
			try {
				await onDisconnected(this).ConfigureAwait(false);
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
			pipe.Dispose();
		}

		private sealed class ServiceConnectionStream : Stream {
			private readonly Stream stream;
			private readonly RutileServiceConnection owner;

			public ServiceConnectionStream(Stream stream, RutileServiceConnection owner) {
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

			public override void Flush() => stream.Flush();
			public override Task FlushAsync(CancellationToken cancellationToken) => stream.FlushAsync(cancellationToken);

			public override int Read(byte[] buffer, int offset, int count) {
				try {
					var read = stream.Read(buffer, offset, count);
					if (read == 0) {
						owner.Disconnected();
					}
					return read;
				}
				catch (IOException) {
					owner.Disconnected();
					throw;
				}
			}

			public override async Task<int> ReadAsync(byte[] buffer, int offset, int count, CancellationToken cancellationToken) {
				try {
					var read = await stream.ReadAsync(buffer, offset, count, cancellationToken).ConfigureAwait(false);
					if (read == 0) {
						owner.Disconnected();
					}
					return read;
				}
				catch (IOException) {
					owner.Disconnected();
					throw;
				}
			}

			public override void Write(byte[] buffer, int offset, int count) {
				try {
					stream.Write(buffer, offset, count);
				}
				catch (IOException) {
					owner.Disconnected();
					throw;
				}
			}

			public override async Task WriteAsync(byte[] buffer, int offset, int count, CancellationToken cancellationToken) {
				try {
					await stream.WriteAsync(buffer, offset, count, cancellationToken).ConfigureAwait(false);
				}
				catch (IOException) {
					owner.Disconnected();
					throw;
				}
			}

			public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();
			public override void SetLength(long value) => throw new NotSupportedException();

			protected override void Dispose(bool disposing) {
				if (disposing) {
					owner.Dispose();
				}
				base.Dispose(disposing);
			}
		}
	}
}
