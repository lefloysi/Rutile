"""Exercise editor recovery, member completion and contextual signatures through Kiln."""

import argparse
from pathlib import Path
import subprocess
import tempfile

from protocol import (Client, at, assert_definition, latest_diagnostics, open_document,
	position, replace_document)


def completion(client, uri, text, marker):
	items = client.request("textDocument/completion", at(uri, text, marker))
	return {item["label"] for item in items}


def main(server, compiler):
	output = Path(__file__).resolve().parents[2] / "build/language-server-tests"
	output.mkdir(parents=True, exist_ok=True)
	with tempfile.TemporaryDirectory(prefix="editor ", dir=output) as directory:
		root = Path(directory)
		library = """namespace tools {
fn answer(i32 value)->i32 {return value;};
fn answer(i64 value)->i32 {return static_cast<i32>(value);};
};
struct Base {i32 inherited;};
struct Item : [Base] {
	i32 field;
	fn read(const Item& this)->i32 {return this.field;};
private:
	i32 hidden;
};
template<typename T>
struct Box {T content;};
"""
		first = (root / "library.rt").as_uri()
		second = (root / "consumer.rt").as_uri()
		client = Client(server, root)
		try:
			capabilities = client.request("initialize", {"rootUri": root.as_uri(), "capabilities": {}})["capabilities"]
			assert "." in capabilities["completionProvider"]["triggerCharacters"]
			client.send("initialized", {})
			open_document(client, first, library)
			version = 0

			def edit(text, library_version=1):
				nonlocal version
				version += 1
				if version == 1:
					open_document(client, second, text, version)
				else:
					replace_document(client, second, text, version)
				client.ready({first: library_version, second: version})

			for expression in ("item.", "item.fi", "pointer->", "tools::", "box."):
				source = "fn main()->i32 {Item item;Item* pointer=&item;Box<i32> box;" + expression + ";return tools::answer(42);};\n"
				edit(source)
				marker = ";return tools"
				names = completion(client, second, source, marker)
				if expression == "tools::":
					assert "answer" in names, names
				elif expression == "box.":
					assert "content" in names, names
				else:
					assert "field" in names and "hidden" not in names, names
					if expression != "item.fi":
						assert {"inherited", "read"} <= names, names
				assert latest_diagnostics(client, second, version), expression
			print("incomplete member/qualified/template/pointer completion with access filtering: PASS", flush=True)

			# A missing operand must not hide a later bound use or declaration.
			source = "fn main()->i32 {i32 value=42; +; return value;};\nfn after()->i32 {return 1;};\n"
			edit(source)
			assert_definition(client, second, source, "value;", second, source, "value=42")
			symbols = client.request("textDocument/documentSymbol", {"textDocument": {"uri": second}})
			assert any(item["name"] == "after" for item in symbols), symbols
			print("syntax recovery retains later declarations and bound uses: PASS", flush=True)

			# Incomplete calls expose compiler overload candidates, without claiming a selection.
			source = "fn main()->i32 {tools::answer("
			edit(source)
			parameters = {"textDocument": {"uri": second}, "position": {"line": 0, "character": len(source)}}
			help_value = client.request("textDocument/signatureHelp", parameters)
			assert help_value and len(help_value["signatures"]) == 2, help_value
			assert "activeSignature" not in help_value, help_value
			print("incomplete call overload signatures: PASS", flush=True)

			# Same template source can refer to different instantiated signatures.
			templates = library + """fn choose(i32 value)->i32 {return value;};
fn choose(i64 value)->i32 {return static_cast<i32>(value);};
template<typename T>
fn forward(T value)->i32 {return choose(value);};
"""
			replace_document(client, first, templates, 2)
			source = "fn main()->i32 {i32 first=20;i64 second=22;return forward<i32>(first)+forward<i64>(second);};\n"
			edit(source, 2)
			help_value = client.request("textDocument/signatureHelp", at(first, templates, "value);};", True))
			assert help_value and len(help_value["signatures"]) == 2, help_value
			assert "activeSignature" not in help_value, help_value
			assert len({item["label"] for item in help_value["signatures"]}) == 2, help_value
			print("template call source retains all instantiated signature contexts: PASS", flush=True)

			client.close()
		finally:
			if client.process.poll() is None:
				client.process.kill()
				client.process.wait()

		# Recovery belongs to editor analysis, never native compilation acceptance.
		invalid = root / "incomplete.rt"
		invalid.write_text("fn main()->i32 {return 42 +;};\n")
		result = subprocess.run([str(compiler), str(invalid), "--output", str(root / "invalid.obj")], capture_output=True, text=True, timeout=30)
		assert result.returncode != 0 and "error" in result.stderr + result.stdout, result
		assert not (root / "invalid.obj").exists()
		print("strict compiler rejects recovered editor syntax: PASS", flush=True)


if __name__ == "__main__":
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--server", type=Path, default=Path("../Kiln/build/bootstrap/bin/kiln.exe"))
	parser.add_argument("--compiler", type=Path, default=Path(".kiln/build/debug/target-727574696c65632e657865/rutilec.exe"))
	options = parser.parse_args()
	main(options.server.resolve(), options.compiler.resolve())
