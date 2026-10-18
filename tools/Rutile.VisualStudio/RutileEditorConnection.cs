using System;
using Newtonsoft.Json.Linq;
using StreamJsonRpc;

namespace Rutile.VisualStudio {
	internal sealed class RutileEditorConnection {
		internal static RutileEditorConnection Instance { get; } = new RutileEditorConnection();
		internal JsonRpc? Rpc { get; private set; }
		internal event EventHandler? Changed;

		internal void Attach(JsonRpc rpc) {
			Rpc = rpc;
			rpc.Disconnected += (_, __) => {
				if (ReferenceEquals(Rpc, rpc)) {
					Rpc = null;
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
