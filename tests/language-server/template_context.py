"""Check concise generic hover and explicitly selected template instantiations."""
import argparse
from pathlib import Path
import tempfile
from protocol import Client, at, open_document, latest_diagnostics


def main(server):
	output = Path(__file__).resolve().parents[2] / "build/language-server-tests"
	output.mkdir(parents=True, exist_ok=True)
	with tempfile.TemporaryDirectory(prefix="template context ", dir=output) as temporary:
		root = Path(temporary)
		uri = (root / "templates.rt").as_uri()
		source = """template<typename T>
struct Box {
	T value;
	fn read(Box& this)->T { return this.value; };
};
fn main()->i32 {
	Box<i32> small;
	Box<i64> wide;
	@small.value=42;
	@wide.value=64;
	wide.read();
	return small.read();
};
"""
		client = Client(server, root)
		try:
			client.request("initialize", {"rootUri": root.as_uri(), "capabilities": {}})
			open_document(client, uri, source)
			client.ready({uri: 1})
			assert latest_diagnostics(client, uri, 1) == []
			location = at(uri, source, "value;")
			generic = client.request("textDocument/hover", location)
			assert generic and len(generic["contents"]["value"]) < 350, generic
			contexts = client.request("rutile/templateContexts", location)
			assert contexts and len(contexts["items"]) == 2, contexts
			assert contexts["selected"] == "", contexts
			body = client.request("rutile/templateContexts", at(uri, source, "value; };"))
			assert body and len(body["items"]) == 2, body
			for primitive in ["i32", "i64"]:
				choice = next(item for item in contexts["items"] if primitive in item["label"])
				selected = client.request("rutile/selectTemplateContext", {**location, "context": choice["key"]})
				assert selected["selected"] == choice["key"], selected
				hover = client.request("textDocument/hover", location)
				assert hover and primitive in hover["contents"]["value"], hover
				assert "\n" not in hover["contents"]["value"], hover
				body_hover = client.request("textDocument/hover", at(uri, source, "value; };"))
				assert body_hover and primitive in body_hover["contents"]["value"], body_hover
			client.request("rutile/selectTemplateContext", {**location, "context": ""})
			assert client.request("textDocument/hover", location) == generic
			print("Concise generic hover and selected i32/i64 template contexts: PASS")
		finally:
			client.close()


if __name__ == "__main__":
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--server", type=Path, default=Path("build/debug/bin/kiln.exe"))
	main(parser.parse_args().server.resolve())
