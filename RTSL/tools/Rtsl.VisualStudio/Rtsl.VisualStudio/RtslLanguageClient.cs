using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio.LanguageServer.Client;
using Microsoft.VisualStudio.Threading;
using Microsoft.VisualStudio.Utilities;

namespace Rtsl.VisualStudio
{
    [ClientName("RTSL")]
    [Export(typeof(ILanguageClient))]
    [ContentType(RtslShaderContentTypes.RtslShader)]
    internal sealed class RtslLanguageClient : ILanguageClient
    {
        private static readonly string LogPath = Path.Combine(Path.GetTempPath(), "rtsl-lsp-client.log");

        private static void Log(string message)
        {
            try
            {
                File.AppendAllText(LogPath, $"{DateTime.Now:HH:mm:ss.fff} {message}{Environment.NewLine}");
            }
            catch
            {
                // Best-effort diagnostics only.
            }
        }

        public string Name => "RTSL Language Server";
        public IEnumerable<string> ConfigurationSections => Array.Empty<string>();
        public object InitializationContext => null;
        public object InitializationOptions => null;
        public IEnumerable<string> FilesToWatch => Array.Empty<string>();
        public bool ShowNotificationOnInitializeFailed => true;

        public event AsyncEventHandler<EventArgs> StartAsync;
        public event AsyncEventHandler<EventArgs> StopAsync;

        static RtslLanguageClient()
        {
            Log("=== RtslLanguageClient type loaded ===");
        }

        public async Task<Connection> ActivateAsync(CancellationToken token)
        {
            Log("ActivateAsync called.");
            var serverPath = Path.Combine(Path.GetDirectoryName(GetType().Assembly.Location) ?? ".", "Rtsl.LanguageServer.exe");
            Log($"Resolved server path: {serverPath}");
            if (!File.Exists(serverPath))
            {
                Log("Server exe NOT FOUND. Returning null (no server will start).");
                return null;
            }

            var info = new ProcessStartInfo(serverPath)
            {
                UseShellExecute = false,
                RedirectStandardInput = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                CreateNoWindow = true
            };

            Process process;
            try
            {
                process = Process.Start(info);
            }
            catch (Exception ex)
            {
                Log($"Process.Start threw: {ex}");
                throw;
            }

            if (process == null)
            {
                Log("Process.Start returned null.");
                throw new InvalidOperationException("Failed to start RTSL language server.");
            }

            Log($"Server process started, PID={process.Id}.");

            await Task.Yield();
            return new Connection(process.StandardOutput.BaseStream, process.StandardInput.BaseStream);
        }

        public async Task OnLoadedAsync()
        {
            Log("OnLoadedAsync called.");
            if (StartAsync != null)
            {
                Log("Invoking StartAsync.");
                await StartAsync.InvokeAsync(this, System.EventArgs.Empty);
                Log("StartAsync invocation completed.");
            }
            else
            {
                Log("StartAsync has no subscribers.");
            }
        }

        public Task<InitializationFailureContext> OnServerInitializeFailedAsync(ILanguageClientInitializationInfo initializationState)
        {
            Log($"OnServerInitializeFailedAsync called. Status={initializationState?.Status}, StatusMessage={initializationState?.StatusMessage}, Exception={initializationState?.InitializationException}");
            return Task.FromResult<InitializationFailureContext>(null);
        }

        public Task OnServerInitializedAsync()
        {
            Log("OnServerInitializedAsync called (server responded to 'initialize').");
            return Task.CompletedTask;
        }
    }
}
