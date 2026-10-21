using System;
using System.Threading;
using Newtonsoft.Json.Linq;
using StreamJsonRpc;

namespace Rutile.VisualStudio {
	internal sealed class RutileEditorConnection {
		internal static RutileEditorConnection Instance { get; } = new RutileEditorConnection();
		private JsonRpc? connection;
		internal JsonRpc? Rpc => Volatile.Read(ref connection);
		internal event EventHandler? Changed;

		internal void Attach(JsonRpc rpc) {
			Interlocked.Exchange(ref connection, rpc);
			rpc.Disconnected += (_, __) => {
				if (ReferenceEquals(Interlocked.CompareExchange(ref connection, null, rpc), rpc)) {
					Changed?.Invoke(this, EventArgs.Empty);
				}
			};
			Changed?.Invoke(this, EventArgs.Empty);
		}

		[JsonRpcMethod("rutile/semanticTokensChanged", UseSingleObjectParameterDeserialization = true)]
		public void SemanticTokensChanged(JToken parameters) {
			Changed?.Invoke(this, EventArgs.Empty);
		}
	}
}
