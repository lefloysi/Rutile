"""Check that IDE-managed Kiln emits the current unsaved analysis for each target."""

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from protocol import Client, open_document, replace_document, write_project


def main(server, command):
	output = Path(__file__).resolve().parents[2] / "build" / "language-server-tests"
	output.mkdir(parents=True, exist_ok=True)
	with tempfile.TemporaryDirectory(prefix="unsaved build \u00e9 ", dir=output) as directory:
		root = Path(directory)
		(root / "app").mkdir()
		(root / "other").mkdir()
		write_project(root, [("app.exe", "executable", ["app"]), ("other.exe", "executable", ["other"])])
		source = root / "app" / "main.rt"
		source.write_text("fn main() -> i32 { return 41; };\n", encoding="utf-8")
		(root / "app" / "helper \u00e9.rt").write_text(
			"fn helper() -> i32 { return 41; };\n", encoding="utf-8")
		(root / "other" / "main.rt").write_text(
			"fn main() -> i32 { return 17; };\n", encoding="utf-8")
		client = Client(server, root)
		try:
			client.request("initialize", {"rootUri": root.as_uri(), "capabilities": {}})
			uri = source.as_uri()
			open_document(client, uri, "fn main() -> i32 { return helper() + 1; };\n")
			client.ready({uri: 1})
			for target, expected in [("other.exe", 17), ("app.exe", 42)]:
				result = subprocess.run([str(command), "run", target], cwd=root,
					capture_output=True, text=True, timeout=15)
				assert result.returncode == expected, result.stdout + result.stderr
			assert "return 41" in source.read_text(), "The editor overlay must not rewrite disk."

			# Disk remains valid. The build must report the unsaved source's actual failure.
			replace_document(client, uri,
				"fn take(var i32* value); fn main() -> i32 { take(true); return 0; };\n", 2)
			client.ready({uri: 2})
			result = subprocess.run([str(command), "run", "app.exe"], cwd=root,
				capture_output=True, text=True, timeout=15)
			assert result.returncode == 1
			assert "no applicable function for call to 'take'" in result.stderr, result.stderr
			assert "note: candidate 'take'" in result.stderr, result.stderr

			# Recovery retains the valid return expression, but the missing brace
			# must still block cached native emission and execution of an older binary.
			replace_document(client, uri, "fn main() -> i32 { return 42;", 3)
			client.ready({uri: 3})
			result = subprocess.run([str(command), "run", "app.exe"], cwd=root,
				capture_output=True, text=True, timeout=15)
			assert result.returncode == 1, result.stdout + result.stderr
			assert "error: expected closing delimiter" in result.stderr, result.stderr
			assert "main.rt:1:" in result.stderr, result.stderr
			assert "return 41" in source.read_text(), "Malformed overlays must leave valid disk input untouched."
			client.close()
		finally:
			if client.process.poll() is None:
				client.process.kill()
				client.process.wait(timeout=5)
	print("Cached per-target unsaved builds, UTF-8 disk input/output and diagnostic replay: PASS")


if __name__ == "__main__":
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--server", type=Path, default=Path("../Kiln/build/bootstrap/bin/kiln.exe"))
	parser.add_argument("--command", type=Path, default=Path("../Kiln/build/bootstrap/bin/kiln.exe"))
	options = parser.parse_args()
	main(options.server.resolve(), options.command.resolve())
