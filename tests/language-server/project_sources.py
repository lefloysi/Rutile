"""Verify the repository's real compiler types use the configured editor source graph."""

import argparse
from pathlib import Path
import shutil
import tempfile

from protocol import Client, at, latest_diagnostics, open_document, position


ROOT = Path(__file__).resolve().parents[2]


def check(server, directory, configured):
	file = directory / "kiln/compiler/src/driver/analysis.rt"
	text = file.read_text(encoding="utf-8")
	uri = file.as_uri()
	client = Client(server, directory)
	try:
		client.request("initialize", {"rootUri": directory.as_uri(), "capabilities": {}}, timeout=30)
		open_document(client, uri, text)
		client.ready({uri: 1}, timeout=120)
		diagnostics = latest_diagnostics(client, uri, 1)
		locations = client.request("textDocument/definition", at(uri, text, "SourceManager sources;"))
		if not configured:
			assert any("unknown type name 'SourceManager'" in item["message"] for item in diagnostics), diagnostics
			assert not locations, locations
		else:
			assert not diagnostics, diagnostics
			target = directory / "kiln/compiler/src/frontend/source.rt"
			expected = position(target.read_text(encoding="utf-8"), "SourceManager {")
			assert locations and all(item["uri"] == target.as_uri() and item["range"]["start"] == expected for item in locations), locations
		client.close()
	finally:
		if client.process.poll() is None:
			client.process.kill()
			client.process.wait(timeout=5)


def main(server):
	output = ROOT / "build/language-server-tests"
	output.mkdir(parents=True, exist_ok=True)
	with tempfile.TemporaryDirectory(prefix="actual compiler sources ", dir=output) as temporary:
		directory = Path(temporary)
		for source in (ROOT / "kiln").rglob("*.rt"):
			target = directory / source.relative_to(ROOT)
			target.parent.mkdir(parents=True, exist_ok=True)
			shutil.copyfile(source, target)
		(directory / "example").mkdir()
		(directory / "example/main.rt").write_text("fn main()->i32 {return 42;};\n", encoding="utf-8")
		for source in [ROOT / "kiln/compiler/sources.list", ROOT / "kiln/sources.list"]:
			shutil.copyfile(source, directory / source.relative_to(ROOT))
		configuration = "targets:\n  example:\n    source: example\n    entry: main\n"
		(directory / "rutile.yaml").write_text(configuration, encoding="utf-8")
		check(server, directory, False)
		(directory / "rutile.yaml").write_text(configuration + "analysis:\n  kiln:\n    sources:\n      - kiln/compiler/std/unique.rt\n      - kiln/compiler/std/optional.rt\n      - kiln/compiler/std/buffer.rt\n      - kiln/compiler/std/string_view.rt\n      - kiln/compiler/std/string.rt\n      - kiln/compiler/std/hash.rt\n      - kiln/compiler/std/equal.rt\n      - kiln/compiler/std/unordered_map.rt\n      - kiln/compiler/std/json.rt\n      - manifest: kiln/compiler/sources.list\n        root: kiln/compiler/src\n      - manifest: kiln/sources.list\n        root: kiln/src\n", encoding="utf-8")
		check(server, directory, True)
	print("Actual compiler project grouping, diagnostics and cross-file type definition: PASS")


if __name__ == "__main__":
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--server", type=Path, default=Path("build/debug/bin/kiln.exe"))
	options = parser.parse_args()
	main(options.server.resolve())
