"""Exercise only the native language-server files extracted from the Rutile VSIX."""

import argparse
from pathlib import Path
import sys
import tempfile
import zipfile

sys.dont_write_bytecode = True
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "tests/language-server"))
from protocol import Client, assert_definition, latest_diagnostics, open_document, replace_document, write_project


def main(vsix):
	output = root / "build/vsix-tests/standalone-server"
	output.mkdir(parents=True, exist_ok=True)
	with tempfile.TemporaryDirectory(dir=output) as temporary:
		directory = Path(temporary)
		payload = directory / "extension"
		with zipfile.ZipFile(vsix) as archive:
			archive.extractall(payload)
		assert not (payload / "Kiln.VisualStudio.dll").exists()
		assert not (payload / "kiln.exe").exists()
		project = directory / "project"
		project.mkdir()
		client = Client(payload / "rutile-language-server.exe", project)
		try:
			capabilities = client.request("initialize", {"rootUri": project.as_uri(), "capabilities": {}})["capabilities"]
			assert capabilities["semanticTokensProvider"]["legend"]["tokenTypes"]
			client.send("initialized", {})
			uri = (project / "main.rt").as_uri()
			open_document(client, uri, "fn main()->i32 {return 42;};\n")
			client.ready({uri: 1})
			assert not latest_diagnostics(client, uri, 1)
			assert client.request("textDocument/semanticTokens/full", {"textDocument": {"uri": uri}})["data"]
			replace_document(client, uri, "fn main()->i32 {return missing();};\n", 2)
			client.ready({uri: 2})
			assert any("missing" in item["message"] for item in latest_diagnostics(client, uri, 2))
			replace_document(client, uri, "fn main()->i32 {return 0;};\n", 3)
			client.ready({uri: 3})
			assert not latest_diagnostics(client, uri, 3)
		finally:
			client.close()
		configured = directory / "configured"
		(configured / "app").mkdir(parents=True)
		write_project(configured, [("app.exe", "executable", ["app"])])
		source = "fn main()->i32 {return answer();};\n"
		library = "fn answer()->i32 {return 42;};\n"
		(configured / "app/main.rt").write_text(source)
		(configured / "app/library.rt").write_text(library)
		client = Client(payload / "rutile-language-server.exe", configured)
		try:
			client.request("initialize", {"rootUri": configured.as_uri(), "capabilities": {}})
			uri = (configured / "app/main.rt").as_uri()
			open_document(client, uri, source)
			client.ready({uri: 1})
			assert not latest_diagnostics(client, uri, 1)
			assert_definition(client, uri, source, "answer", (configured / "app/library.rt").as_uri(), library, "answer")
		finally:
			client.close()
	print("Standalone Rutile VSIX: initialize, semantic tokens, edit diagnostics, recovery and configured project navigation PASS")


if __name__ == "__main__":
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--vsix", type=Path, required=True)
	main(parser.parse_args().vsix.resolve())
