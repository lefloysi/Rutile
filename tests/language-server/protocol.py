"""Exercise Kiln's Rutile language server through its real editor connection."""

import argparse
import json
from pathlib import Path
import queue
import statistics
import subprocess
import tempfile
import threading
import time


class Client:
	def __init__(self, executable, directory):
		self.process = subprocess.Popen([str(executable), "language-server", "--stdio"], stdin=subprocess.PIPE,
			stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=directory)
		self.messages = queue.Queue()
		self.notifications = []
		self.next_id = 0
		self.reader = threading.Thread(target=self.read, daemon=True)
		self.reader.start()

	def read(self):
		try:
			while True:
				headers = {}
				while True:
					line = self.process.stdout.readline()
					if not line:
						return
					assert line.endswith(b"\r\n"), line
					if line == b"\r\n":
						break
					name, value = line.decode("ascii").split(":", 1)
					headers[name.lower()] = value.strip()
				length = int(headers["content-length"])
				body = self.process.stdout.read(length)
				assert len(body) == length
				self.messages.put(json.loads(body))
		except BaseException as error:
			self.messages.put(error)

	def send(self, method, parameters=None, identifier=None):
		message = {"jsonrpc": "2.0", "method": method}
		if parameters is not None:
			message["params"] = parameters
		if identifier is not None:
			message["id"] = identifier
		body = json.dumps(message, ensure_ascii=False).encode("utf-8")
		self.process.stdin.write(f"Content-Length: {len(body)}\r\n\r\n".encode("ascii") + body)
		self.process.stdin.flush()

	def request(self, method, parameters=None, timeout=10):
		self.next_id += 1
		identifier = self.next_id
		self.send(method, parameters, identifier)
		deadline = time.monotonic() + timeout
		while True:
			message = self.messages.get(timeout=max(0.001, deadline - time.monotonic()))
			if isinstance(message, BaseException):
				raise message
			if message.get("id") == identifier:
				assert "error" not in message, message
				return message["result"]
			self.notifications.append(message)

	def ready(self, versions, timeout=20):
		deadline = time.monotonic() + timeout
		while time.monotonic() < deadline:
			status = self.request("rutile/indexStatus")
			indexed = {document["uri"]: document["indexedVersion"] for document in status["documents"]}
			if status["indexedGeneration"] == status["generation"] and all(indexed.get(uri) == version for uri, version in versions.items()):
				return status
			time.sleep(0.02)
		raise AssertionError(f"Index did not become ready: {status}")

	def close(self):
		if self.process.poll() is None:
			self.request("shutdown", timeout=5)
			self.send("exit")
			self.process.wait(timeout=5)
		assert self.process.returncode == 0, self.process.stderr.read().decode()


def position(text, needle, last=False):
	offset = text.rindex(needle) if last else text.index(needle)
	prefix = text[:offset]
	return {"line": prefix.count("\n"), "character": len(prefix.rsplit("\n", 1)[-1].encode("utf-16-le")) // 2}


def at(uri, text, needle, last=False):
	return {"textDocument": {"uri": uri}, "position": position(text, needle, last)}


def open_document(client, uri, text, version=1):
	client.send("textDocument/didOpen", {"textDocument": {"uri": uri, "languageId": "rutile", "version": version, "text": text}})


def replace_document(client, uri, text, version):
	client.send("textDocument/didChange", {"textDocument": {"uri": uri, "version": version}, "contentChanges": [{"text": text}]})


def assert_definition(client, uri, source, needle, target_uri, target, target_needle, last=False):
	locations = client.request("textDocument/definition", at(uri, source, needle, last))
	expected = {"uri": target_uri, "start": position(target, target_needle)}
	assert locations, (needle, expected, locations)
	assert all(item["uri"] == target_uri and item["range"]["start"] == expected["start"] for item in locations), (needle, expected, locations)


def template_navigation(client, first, second, library):
	extended = library + """template<typename T>
struct Box {
	T value;
	fn get(const Box& this)->T {return T(this.value);};
};
template<typename T>
fn echo(T value)->T {return $value;};
fn pick(i32 value)->i32 {return value;};
fn pick(i64 value)->i32 {return static_cast<i32>(value);};
"""
	source = """using IntBox = Box<i32>;
fn main()->i32 {
	IntBox box;
	@box.value=40;
	i32 input=2;
	var moved=echo<Box<i32>>($box);
	return moved.get()+pick(input);
};
"""
	replace_document(client, first, extended, 2)
	replace_document(client, second, source, 2)
	client.ready({first: 2, second: 2})
	assert_definition(client, second, source, "pick(input)", first, extended, "pick(i32")
	completion = client.request("textDocument/completion", at(second, source, "pick(input)"))
	assert any(item["label"] == "input" for item in completion), completion
	assert sum(item["label"] == "pick" for item in completion) == 2, completion
	signature = client.request("textDocument/signatureHelp", at(second, source, "input);"))
	assert signature and "pick" in signature["signatures"][0]["label"], signature
	assert len(signature["signatures"][0]["parameters"]) == 1, signature
	assert signature["activeParameter"] == 0, signature
	assert signature["signatures"][0]["parameters"][0]["label"] in signature["signatures"][0]["label"], signature
	method_signature = client.request("textDocument/signatureHelp", at(second, source, ")+pick"))
	assert method_signature and method_signature["signatures"][0]["label"] == "get()", method_signature
	assert method_signature["signatures"][0]["parameters"] == [], method_signature


	assert_definition(client, second, source, "get()+", first, extended, "get(const")
	assert_definition(client, second, source, "value=40", first, extended, "value;\n\tfn get")
	assert_definition(client, second, source, "echo<Box", first, extended, "echo(T")
	assert_definition(client, second, source, "Box<i32>>", first, extended, "Box {")
	assert_definition(client, second, source, "IntBox box", second, source, "IntBox =")
	assert_definition(client, first, extended, "T value;", first, extended, "T>\nstruct Box")
	references = client.request("textDocument/references", {**at(first, extended, "get(const"), "context": {"includeDeclaration": False}})
	assert any(item["uri"] == second and item["range"]["start"] == position(source, "get()+") for item in references), references
	print("template, overload, member, alias and type-argument navigation: PASS")
	return extended


def latest_diagnostics(client, uri, version):
	matching = [message["params"] for message in client.notifications
		if message.get("method") == "textDocument/publishDiagnostics"
		and message["params"].get("uri") == uri and message["params"].get("version") == version]
	assert matching, (uri, version, client.notifications)
	return matching[-1]["diagnostics"]


def diagnostic_versions(client, first, second, library, valid_source):
	invalid = "fn main()->i32 {return answer();};\n"
	replace_document(client, second, invalid, 5)
	client.ready({first: 2, second: 5})
	diagnostics = latest_diagnostics(client, second, 5)
	assert diagnostics, diagnostics
	assert any("answer" in item["message"] for item in diagnostics), diagnostics
	related = [note for item in diagnostics for note in item.get("relatedInformation", [])]
	assert any(note["location"]["uri"] == first and note["location"]["range"]["start"] == position(library, "answer") for note in related), diagnostics
	assert all(item["range"]["start"]["line"] == 0 and item["range"]["end"]["character"] >= item["range"]["start"]["character"] for item in diagnostics)

	# Supersede another failing revision immediately. Only the latest publication
	# is used, and clearing diagnostics carries its matching document version.
	replace_document(client, second, invalid, 6)
	replace_document(client, second, valid_source, 7)
	client.ready({first: 2, second: 7})
	assert latest_diagnostics(client, second, 7) == []
	status = client.request("rutile/indexStatus")
	assert next(document for document in status["documents"] if document["uri"] == second)["indexedVersion"] == 7
	print("diagnostic declaration notes, ranges and superseded versions: PASS")


def independent_targets(executable, output):
	with tempfile.TemporaryDirectory(prefix="rutile targets ", dir=output) as directory:
		root = Path(directory)
		first = root / "first"
		second = root / "second"
		first.mkdir()
		second.mkdir()
		(root / "rutile.yaml").write_text("targets:\n  first:\n    source: first\n    entry: main\n  second:\n    source: second\n    entry: main\n", encoding="utf-8")
		first_source = "fn compute()->i32 {return 20;};\nfn main()->i32 {return compute();};\n"
		second_source = "fn compute()->i32 {return 22;};\nfn main()->i32 {return compute();};\n"
		(first / "main.rt").write_text(first_source, encoding="utf-8")
		(second / "main.rt").write_text(second_source, encoding="utf-8")
		first_uri = (first / "main.rt").as_uri()
		second_uri = (second / "main.rt").as_uri()
		client = Client(executable, root)
		try:
			client.request("initialize", {"rootUri": root.as_uri(), "capabilities": {}})
			client.send("initialized", {})
			open_document(client, first_uri, first_source)
			open_document(client, second_uri, second_source)
			client.ready({first_uri: 1, second_uri: 1})
			assert_definition(client, first_uri, first_source, "compute();", first_uri, first_source, "compute()->")
			assert_definition(client, second_uri, second_source, "compute();", second_uri, second_source, "compute()->")
			assert latest_diagnostics(client, first_uri, 1) == []
			assert latest_diagnostics(client, second_uri, 1) == []
			updated = "\n" + second_source.replace("22", "42")
			replace_document(client, second_uri, updated, 2)
			client.ready({first_uri: 1, second_uri: 2})
			assert_definition(client, first_uri, first_source, "compute();", first_uri, first_source, "compute()->")
			assert_definition(client, second_uri, updated, "compute();", second_uri, updated, "compute()->")
			assert latest_diagnostics(client, second_uri, 2) == []
			client.close()
			print("independent project target scopes and unsaved overlay navigation: PASS")
		finally:
			if client.process.poll() is None:
				client.process.kill()
				client.process.wait()


def main(executable):
	output = Path(__file__).resolve().parents[2] / "build" / "language-server-tests"
	output.mkdir(parents=True, exist_ok=True)
	with tempfile.TemporaryDirectory(prefix="rutile lsp ", dir=output) as directory:
		root = Path(directory)
		first = (root / "library é.rt").as_uri()
		second = (root / "consumer.rt").as_uri()
		library = "fn answer(i32 value)->i32 {return value;};\n"
		consumer = "fn main()->i32 {i32 value=42;return answer(value);};\n"
		client = Client(executable, root)
		try:
			capabilities = client.request("initialize", {"rootUri": root.as_uri(), "capabilities": {}})["capabilities"]
			assert capabilities["definitionProvider"] and capabilities["referencesProvider"]
			assert "renameProvider" not in capabilities
			assert capabilities["completionProvider"]["resolveProvider"] is False
			assert capabilities["signatureHelpProvider"]["triggerCharacters"] == ["(", ","]
			client.send("initialized", {})
			open_document(client, first, library)
			open_document(client, second, consumer)
			client.ready({first: 1, second: 1})

			definition = client.request("textDocument/definition", at(second, consumer, "answer"))
			assert definition and definition[0]["uri"] == first, definition
			assert definition[0]["range"]["start"] == position(library, "answer"), definition
			stable_latencies = []
			for _ in range(50):
				start = time.perf_counter()
				indexed_target = client.request("textDocument/definition", at(second, consumer, "answer"))
				stable_latencies.append((time.perf_counter() - start) * 1000)
				assert indexed_target == definition, indexed_target
			print(f"stable bound F12 median={statistics.median(stable_latencies):.3f}ms max={max(stable_latencies):.3f}ms")
			declaration = client.request("textDocument/declaration", at(second, consumer, "answer"))
			assert declaration == definition
			references = client.request("textDocument/references", {**at(first, library, "answer"), "context": {"includeDeclaration": True}})
			assert {item["uri"] for item in references} == {first, second}, references
			hover = client.request("textDocument/hover", at(second, consumer, "answer"))
			assert "answer" in hover["contents"]["value"], hover
			symbols = client.request("textDocument/documentSymbol", {"textDocument": {"uri": first}})
			assert any(item["name"] == "answer" for item in symbols), symbols
			assert client.request("workspace/symbol", {"query": "answer"})

			# Local symbols with identical spellings must bind by compiler identity.
			local = client.request("textDocument/definition", at(second, consumer, "value", True))
			assert local and local[0]["uri"] == second, local
			assert local[0]["range"]["start"] == position(consumer, "value"), local
			library = template_navigation(client, first, second, library)

			# Full replacement invalidates old bindings until the new snapshot is ready.
			shadowed = "fn main()->i32 {i32 answer=42;return answer;};\n"
			replace_document(client, second, shadowed, 3)
			client.ready({first: 2, second: 3})
			binding = client.request("textDocument/definition", at(second, shadowed, "answer", True))
			assert binding and binding[0]["uri"] == second, binding
			assert binding[0]["range"]["start"] == position(shadowed, "answer"), binding

			# An out-of-order edit must not overwrite the latest document.
			replace_document(client, second, consumer, 1)
			status = client.request("rutile/indexStatus")
			assert next(document for document in status["documents"] if document["uri"] == second)["version"] == 3

			# UTF-16 positions include supplementary characters before the queried name.
			unicode_source = 'fn main()->i32 {const char* text="😀";i32 result=42;return result;};\n'
			replace_document(client, second, unicode_source, 4)
			client.ready({first: 2, second: 4})
			binding = client.request("textDocument/definition", at(second, unicode_source, "result", True))
			assert binding and binding[0]["range"]["start"] == position(unicode_source, "result"), binding
			diagnostic_versions(client, first, second, library, unicode_source)

			# Queries remain independent of a pending large reanalysis.
			large = "\n" + unicode_source + "".join(f"fn item_{index}()->i32 {{return {index};}};\n" for index in range(10000))
			replace_document(client, second, large, 8)
			latencies = []
			for _ in range(50):
				start = time.perf_counter()
				client.request("textDocument/definition", at(second, large, "result", True), timeout=2)
				latencies.append((time.perf_counter() - start) * 1000)
			print(f"pending-analysis response median={statistics.median(latencies):.3f}ms max={max(latencies):.3f}ms (may be unindexed)")

			client.send("$/cancelRequest", {"id": 999999})
			client.send("textDocument/didClose", {"textDocument": {"uri": first}})
			client.close()
			print("language-server protocol: PASS")
		finally:
			if client.process.poll() is None:
				client.process.kill()
				client.process.wait()
	independent_targets(executable, output)


if __name__ == "__main__":
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--server", type=Path, default=Path("build/release/bin/kiln.exe"))
	main(parser.parse_args().server.resolve())
