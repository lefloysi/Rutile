"""Verify cached compiler-bound semantic token classes and UTF-16 ranges."""
import argparse
from pathlib import Path
import tempfile
from protocol import Client, open_document, position, replace_document, latest_diagnostics


def decode(result, legend):
	data = result["data"]
	assert len(data) % 5 == 0
	line = character = 0
	result = []
	for index in range(0, len(data), 5):
		delta_line, delta_character, length, kind, modifiers = data[index:index + 5]
		assert length > 0 and kind < len(legend)
		line += delta_line
		character = delta_character if delta_line else character + delta_character
		result.append((line, character, length, legend[kind], modifiers))
	return result


def token_at(tokens, source, needle, expected):
	location = position(source, needle)
	matching = [token for token in tokens if token[:2] == (location["line"], location["character"])]
	assert len(matching) == 1 and matching[0][3] == expected, (needle, expected, matching)
	return matching[0]


def main(server):
	output = Path(__file__).resolve().parents[2] / "build/language-server-tests"
	output.mkdir(parents=True, exist_ok=True)
	with tempfile.TemporaryDirectory(prefix="semantic ", dir=output) as directory:
		root = Path(directory)
		uri = (root / "highlight.rt").as_uri()
		source = """struct Foo {
	i32 field;
public:
	fn operator+(Foo& this, i32 other)->i32;
private:
	i32 hidden;
};
using Alias = Foo;
template<typename T> struct Box { T item; };
template<typename U> struct Shadow { i32 U; };
fn shadow_name(i32 T)->i32 { return T; };
fn read(Foo& input)->i32 { return input.field; };
fn automatic(auto Foo& input)->i32 { return input.field; };
fn casted()->i32 { return static_cast<i32>(42); };
fn names()->i32 { i32 auto = 1; i32 public = 2; return public; };
fn main()->i32 { Foo value; "emoji 😀"; return read(value); };
"""
		client = Client(server, root)
		try:
			capabilities = client.request("initialize", {"rootUri": root.as_uri(), "capabilities": {}})["capabilities"]
			provider = capabilities["semanticTokensProvider"]
			assert provider["full"] is True and provider["range"] is True
			legend = provider["legend"]["tokenTypes"]
			open_document(client, uri, source, 1)
			client.ready({uri: 1})
			assert latest_diagnostics(client, uri, 1) == []
			full = client.request("textDocument/semanticTokens/full", {"textDocument": {"uri": uri}})
			tokens = decode(full, legend)
			for needle, expected in [("Foo {", "struct"), ("Foo;", "struct"), ("Alias =", "type"), ("T> struct", "typeParameter"), ("T item", "typeParameter"), ("input)->", "parameter"), ("field;", "property"), ("read(Foo", "function"), ("i32 field", "type"), ("public:", "keyword"), ("private:", "keyword"), ("operator+", "keyword"), ("auto Foo", "keyword"), ("static_cast<i32>", "keyword"), ("auto =", "variable"), ("public =", "variable")]:
				token_at(tokens, source, needle, expected)
			token_at(tokens, source, "T)->i32", "parameter")
			token_at(tokens, source, "U> struct", "typeParameter")
			shadow = position(source, "U; };")
			assert not any(token[:2] == (shadow["line"], shadow["character"]) and token[3] == "typeParameter" for token in tokens)
			literal = token_at(tokens, source, '"emoji', "string")
			assert literal[2] == len('"emoji \U0001f600"'.encode("utf-16-le")) // 2
			call = token_at(tokens, source, "read(value)", "function")
			requested = {"start": {"line": call[0], "character": call[1]}, "end": {"line": call[0], "character": call[1] + 4}}
			ranged = decode(client.request("textDocument/semanticTokens/range", {"textDocument": {"uri": uri}, "range": requested}), legend)
			assert len(ranged) == 1 and ranged[0][:4] == call[:4], ranged
			assert full == client.request("textDocument/semanticTokens/full", {"textDocument": {"uri": uri}})
			replace_document(client, uri, source + "\n", 2)
			client.ready({uri: 2})
			assert decode(client.request("textDocument/semanticTokens/full", {"textDocument": {"uri": uri}}), legend) == tokens
		finally:
			client.close()
	print("semantic tokens: bindings, UTF-16, range, cache and versions PASS")


if __name__ == "__main__":
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--server", type=Path, default=Path("build/debug/bin/kiln.exe"))
	main(parser.parse_args().server.resolve())
