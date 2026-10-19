"""Measure real indexed F12 across a multi-file project and test disk invalidation."""

import argparse
from pathlib import Path
import statistics
import tempfile
import time

from protocol import Client, at, open_document, position, write_project


def main(server):
	output = Path(__file__).resolve().parents[2] / "build/language-server-tests"
	output.mkdir(parents=True, exist_ok=True)
	with tempfile.TemporaryDirectory(prefix="navigation ", dir=output) as directory:
		root = Path(directory)
		(root / "src").mkdir()
		write_project(root, [("app", "executable", ["src"])])
		for file in range(100):
			(root / "src" / f"library_{file:03}.rt").write_text("".join(
				f"fn item_{file}_{number}()->i32 {{return {number};}};\n" for number in range(100)))
		consumer = "fn main()->i32 {return item_99_42();};\n"
		path = root / "src/main.rt"
		path.write_text(consumer)
		client = Client(server, root)
		try:
			client.request("initialize", {"rootUri": root.as_uri(), "capabilities": {}})
			uri = path.as_uri()
			open_document(client, uri, consumer)
			status = client.ready({uri: 1}, timeout=60)
			query = at(uri, consumer, "item_99_42")
			target = (root / "src/library_099.rt").as_uri()
			expected = position((root / "src/library_099.rt").read_text(), "item_99_42")
			latencies = []
			for _ in range(200):
				start = time.perf_counter_ns()
				locations = client.request("textDocument/definition", query)
				latencies.append((time.perf_counter_ns() - start) / 1e6)
				assert locations and all(item["uri"] == target and item["range"]["start"] == expected for item in locations), locations
			print(f"10,000 functions / 101 files indexed F12: median={statistics.median(latencies):.3f}ms p95={sorted(latencies)[189]:.3f}ms max={max(latencies):.3f}ms", flush=True)
			print(f"Background index build={status['indexBuildNanoseconds'] / 1e6:.3f}ms", flush=True)

			library = root / "src/library_099.rt"
			library.write_text("\n" + library.read_text())
			client.send("workspace/didChangeWatchedFiles", {"changes": [{"uri": library.as_uri(), "type": 2}]})
			client.ready({uri: 1}, timeout=60)
			locations = client.request("textDocument/definition", query)
			assert locations and locations[0]["range"]["start"]["line"] == expected["line"] + 1, locations
			library.unlink()
			client.send("workspace/didChangeWatchedFiles", {"changes": [{"uri": library.as_uri(), "type": 3}]})
			client.ready({uri: 1}, timeout=60)
			assert not client.request("textDocument/definition", query)
			library.write_text("fn item_99_42()->i32 {return 42;};\n")
			client.send("workspace/didChangeWatchedFiles", {"changes": [{"uri": library.as_uri(), "type": 1}]})
			client.ready({uri: 1}, timeout=60)
			locations = client.request("textDocument/definition", query)
			assert locations and locations[0]["uri"] == target and locations[0]["range"]["start"]["line"] == 0, locations
			print("watched disk source change/delete/create invalidates navigation: PASS", flush=True)

			(root / "other").mkdir()
			(root / "other/main.rt").write_text("fn main()->i32 {return 0;};\n")
			write_project(root, [("app", "executable", ["other"])])
			client.send("workspace/didChangeWatchedFiles", {"changes": [{"uri": (root / "kiln.rt").as_uri(), "type": 2}]})
			client.ready({uri: 1}, timeout=60)
			assert not client.request("textDocument/definition", query)
			print("watched project configuration changes rebuild target boundaries: PASS", flush=True)
			client.close()
		finally:
			if client.process.poll() is None:
				client.process.kill()
				client.process.wait()


if __name__ == "__main__":
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--server", type=Path, default=Path("../Kiln/build/bootstrap/bin/kiln.exe"))
	main(parser.parse_args().server.resolve())
