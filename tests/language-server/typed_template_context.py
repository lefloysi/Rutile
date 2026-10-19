"""Typed template contexts use canonical instantiation without altering build analysis."""
import argparse
from pathlib import Path
import tempfile
from protocol import Client, at, latest_diagnostics, open_document


def main(server):
	output = Path(__file__).resolve().parents[2] / "build/language-server-tests"
	output.mkdir(parents=True, exist_ok=True)
	with tempfile.TemporaryDirectory(prefix="typed template arguments ", dir=output) as temporary:
		root = Path(temporary)
		uri = (root / "templates.rt").as_uri()
		source = """template<typename T>
struct Box {
	T value;
	fn read(Box& this)->T { return this.value; };
};
template<typename T>
struct Other { T other; };
fn main()->i32 {return 42;};
"""
		client = Client(server, root)
		try:
			client.request("initialize", {"rootUri": root.as_uri(), "capabilities": {}})
			open_document(client, uri, source)
			client.ready({uri: 1})
			location = at(uri, source, "value;")
			initial = client.request("rutile/templateContexts", location)
			assert initial["template"]["parameters"] == ["T"], initial
			assert initial["template"]["range"]["start"]["line"] == 0, initial
			assert initial["items"] == [], initial
			client.request("rutile/selectTemplateContext", {**location, "arguments": "u16"})
			client.ready({uri: 1})
			selected = client.request("rutile/templateContexts", location)
			assert not selected.get("error") and "u16" in selected.get("label", ""), selected
			hover = client.request("textDocument/hover", location)
			assert hover and "u16" in hover["contents"]["value"], hover
			assert latest_diagnostics(client, uri, 1) == []

			# The compiler resolves user text in the declaration scope and reports its own failure.
			for invalid in ["MissingType", "i32>; using Injected = i32; //"]:
				client.request("rutile/selectTemplateContext", {**location, "arguments": invalid})
				client.ready({uri: 1})
				failed = client.request("rutile/templateContexts", location)
				assert failed.get("error"), failed
				assert not failed.get("label"), failed
				assert latest_diagnostics(client, uri, 1) == [], "Context input must not poison actual program diagnostics."
			assert "MissingType" not in source

			# Queued requests are generation-checked; only the latest arguments can become current.
			client.request("rutile/selectTemplateContext", {**location, "arguments": "u08"})
			client.request("rutile/selectTemplateContext", {**location, "arguments": "i64"})
			client.ready({uri: 1})
			selected = client.request("rutile/templateContexts", location)
			assert "i64" in selected.get("label", "") and not selected.get("error"), selected

			# Results belong to the declaration as well as its arguments, including pending work.
			other = at(uri, source, "other;")
			unselected = client.request("rutile/templateContexts", other)
			assert unselected["arguments"] == "" and not unselected.get("label") and not unselected.get("error"), unselected
			client.request("rutile/selectTemplateContext", {**other, "arguments": "i64"})
			pending = client.request("rutile/templateContexts", other)
			assert not pending.get("label") or pending["label"].startswith("Other<"), pending
			assert not pending.get("error"), pending
			client.ready({uri: 1})
			selected_other = client.request("rutile/templateContexts", other)
			assert selected_other.get("label", "").startswith("Other<i64>"), selected_other
			old = client.request("rutile/templateContexts", location)
			assert old["arguments"] == "" and not old.get("label") and not old.get("error"), old
			client.request("rutile/selectTemplateContext", {**location, "arguments": "i64"})
			client.ready({uri: 1})

			# An insertion before the declaration keeps the same compiler declaration selected.
			client.send("textDocument/didChange", {"textDocument": {"uri": uri, "version": 2}, "contentChanges": [{"range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 0}}, "text": "\n"}]})
			source = "\n" + source
			location = at(uri, source, "value;")
			client.ready({uri: 2})
			selected = client.request("rutile/templateContexts", location)
			assert "i64" in selected.get("label", "") and selected["template"]["range"]["start"]["line"] == 1, selected

			client.request("rutile/selectTemplateContext", {**location, "context": ""})
			client.ready({uri: 2})
			reset = client.request("rutile/templateContexts", location)
			assert reset["items"] == [] and reset["arguments"] == "" and not reset.get("label"), reset
			assert latest_diagnostics(client, uri, 2) == []
			print("Unseen typed arguments, compiler errors, generation safety, edit tracking and generic reset: PASS")
		finally:
			client.close()


if __name__ == "__main__":
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--server", type=Path, default=Path("../Kiln/build/bootstrap/bin/kiln.exe"))
	main(parser.parse_args().server.resolve())
