"""Measure cached semantic colors while the real compiler project is edited."""
import argparse
from pathlib import Path
import shutil
import statistics
import subprocess
import tempfile
import time
from protocol import Client, open_document, position
from semantic_tokens import decode

ROOT = Path(__file__).resolve().parents[2]


def ready(client, versions, timeout):
	deadline = time.monotonic() + timeout
	phase = None
	while time.monotonic() < deadline:
		status = client.request("rutile/indexStatus")
		if status.get("analysisPhase") != phase:
			phase = status.get("analysisPhase")
			print("Index phase:", phase, "elapsed ns:", status.get("phaseNanoseconds"), flush=True)
		if phase == "failed":
			raise AssertionError(("Worker failed", status, client.notifications[-3:]))
		indexed = {document["uri"]: document["indexedVersion"] for document in status["documents"]}
		if status["indexedGeneration"] == status["generation"] and all(indexed.get(uri) == version for uri, version in versions.items()):
			return status
		time.sleep(0.02)
	raise AssertionError(("Index did not become ready", status, client.notifications[-3:]))


def main(server, source_ref=None, timeout=45):
	output = ROOT / "build/language-server-tests"
	output.mkdir(parents=True, exist_ok=True)
	with tempfile.TemporaryDirectory(prefix="editing compiler ", dir=output) as temporary:
		root = Path(temporary)
		if source_ref:
			names = subprocess.check_output(["git", "ls-tree", "-r", "--name-only", source_ref, "kiln"], cwd=ROOT, text=True).splitlines()
			for name in names:
				if not name.endswith(".rt") and name not in ["kiln/compiler/sources.list", "kiln/sources.list"]:
					continue
				target = root / name
				target.parent.mkdir(parents=True, exist_ok=True)
				target.write_bytes(subprocess.check_output(["git", "show", source_ref + ":" + name], cwd=ROOT))
		else:
			for source in (ROOT / "kiln").rglob("*.rt"):
				target = root / source.relative_to(ROOT)
				target.parent.mkdir(parents=True, exist_ok=True)
				shutil.copyfile(source, target)
			for name in ["kiln/compiler/sources.list", "kiln/sources.list"]:
				shutil.copyfile(ROOT / name, root / name)
		configuration = "targets:\n  example:\n    source: example\n    entry: main\nanalysis:\n  compiler:\n    sources:\n" + "".join("      - kiln/compiler/std/" + name + ".rt\n" for name in ["unique", "optional", "buffer", "string_view", "string", "hash", "equal", "unordered_map", "json"]) + "      - manifest: kiln/compiler/sources.list\n        root: kiln/compiler/src\n      - manifest: kiln/sources.list\n        root: kiln/src\n"
		(root / "example").mkdir()
		(root / "example/main.rt").write_text("fn main()->i32 { return 42; };\n", encoding="utf-8")
		(root / "rutile.yaml").write_text(configuration, encoding="utf-8")
		file = root / "kiln/compiler/src/driver/analysis.rt"
		source = file.read_text(encoding="utf-8")
		uri = file.as_uri()
		client = Client(server, root)
		try:
			provider = client.request("initialize", {"rootUri": root.as_uri(), "capabilities": {}})["capabilities"]["semanticTokensProvider"]
			started = time.perf_counter()
			open_document(client, uri, source, 1)
			status = ready(client, {uri: 1}, timeout)
			print(f"Actual compiler initial index {(time.perf_counter() - started):.3f}s; worker {status['indexBuildNanoseconds'] / 1e9:.3f}s", flush=True)
			parameters = {"textDocument": {"uri": uri}}
			legend = provider["legend"]["tokenTypes"]
			before = decode(client.request("textDocument/semanticTokens/full", parameters), legend)
			type_position = position(source, "SourceManager sources;")
			anchor = next(token for token in before if token[:2] == (type_position["line"], type_position["character"]))
			assert anchor[3] == "struct", anchor
			later = position(source, "source->path();")
			later_anchor = next(token for token in before if token[:2] == (later["line"], later["character"]))
			assert later_anchor[3] == "variable", later_anchor
			# One character breaks a body expression without touching the type anchor.
			caret = position(source, "count();")
			client.send("textDocument/didChange", {"textDocument": {"uri": uri, "version": 2}, "contentChanges": [{"range": {"start": caret, "end": caret}, "text": "x"}]})
			latencies = []
			for _ in range(8):
				started = time.perf_counter()
				current = decode(client.request("textDocument/semanticTokens/full", parameters), legend)
				latencies.append((time.perf_counter() - started) * 1000)
				assert anchor in current and later_anchor in current, ("Unrelated bindings lost color during pending analysis", current[:20])
			print(f"Pending-edit token responses median={statistics.median(latencies):.3f}ms max={max(latencies):.3f}ms", flush=True)
			started = time.perf_counter()
			status = ready(client, {uri: 2}, timeout)
			current = decode(client.request("textDocument/semanticTokens/full", parameters), legend)
			assert anchor in current and later_anchor in current, "Temporary semantic error removed a later unrelated binding classification"
			print(f"Edited compiler analysis {(time.perf_counter() - started):.3f}s; worker {status['indexBuildNanoseconds'] / 1e9:.3f}s; unchanged color retained", flush=True)
		finally:
			client.close()


if __name__ == "__main__":
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--server", type=Path, default=Path("build/debug/bin/kiln.exe"))
	parser.add_argument("--source-ref")
	parser.add_argument("--timeout", type=float, default=45)
	options = parser.parse_args()
	main(options.server.resolve(), options.source_ref, options.timeout)
