"""Verify server-owned token colors and indexing survive persistent compiler reconnects."""

import argparse
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading
import time

sys.dont_write_bytecode = True
from compiler_service import endpoint_for, request


def bounded_request(endpoint, text, timeout=5):
	results = queue.Queue()
	def run():
		try:
			results.put(request(endpoint, text))
		except BaseException as error:
			results.put(error)
	threading.Thread(target=run, daemon=True).start()
	try:
		result = results.get(timeout=timeout)
	except queue.Empty:
		raise AssertionError(f"Compiler request timed out: {text[:80]}")
	if isinstance(result, BaseException):
		raise result
	return result


class Editor:
	def __init__(self, endpoint, root, custom_colors=True):
		self.pipe = open(endpoint + "-lsp", "r+b", buffering=0)
		self.input = os.fdopen(os.dup(self.pipe.fileno()), "rb", buffering=0)
		self.messages = queue.Queue()
		self.notifications = []
		self.identifier = 0
		print("Editor pipe connected; initializing", flush=True)
		capabilities = self.call("initialize", {"rootUri": root.as_uri(), "capabilities": {},
			"initializationOptions": {"rutileSemanticColors": custom_colors}})["capabilities"]
		self.legend = self.call("rutile/semanticTokensLegend", {})["tokenTypes"]
		assert "_" in capabilities["completionProvider"]["triggerCharacters"], capabilities
		if custom_colors:
			assert "semanticTokensProvider" not in capabilities, capabilities
		else:
			assert self.legend == capabilities["semanticTokensProvider"]["legend"]["tokenTypes"], self.legend
		assert self.legend[:19] == ["namespace", "type", "class", "enum", "interface", "struct",
			"typeParameter", "parameter", "variable", "property", "enumMember", "function",
			"method", "keyword", "comment", "string", "number", "operator", "controlKeyword"], self.legend
		self.send("initialized", {})
		print("Editor initialized", flush=True)

	def read(self, identifier):
		try:
			while True:
				headers = bytearray()
				while not headers.endswith(b"\r\n\r\n"):
					value = self.input.read(1)
					if not value:
						raise EOFError("Editor connection closed")
					headers += value
				length = int(next(line.split(b":", 1)[1] for line in headers.split(b"\r\n")
					if line.lower().startswith(b"content-length:")))
				body = bytearray()
				while len(body) < length:
					value = self.input.read(length - len(body))
					if not value:
						raise EOFError("Editor response was truncated")
					body += value
				message = json.loads(body)
				if message.get("method") == "textDocument/publishDiagnostics":
					self.notifications.append(message["params"])
				if message.get("id") == identifier:
					self.messages.put(message)
					return
		except BaseException as error:
			self.messages.put(error)

	def send(self, method, parameters, identifier=None):
		message = {"jsonrpc": "2.0", "method": method, "params": parameters}
		if identifier is not None:
			message["id"] = identifier
		body = json.dumps(message).encode("utf-8")
		packet = f"Content-Length: {len(body)}\r\n\r\n".encode("ascii") + body
		while packet:
			written = self.pipe.write(packet)
			if not written:
				raise EOFError("Editor connection stopped accepting data")
			packet = packet[written:]

	def call(self, method, parameters):
		self.identifier += 1
		self.send(method, parameters, self.identifier)
		self.reader = threading.Thread(target=self.read, args=(self.identifier,), daemon=True)
		self.reader.start()
		deadline = time.monotonic() + 10
		while True:
			try:
				message = self.messages.get(timeout=max(.001, deadline - time.monotonic()))
			except queue.Empty:
				raise AssertionError(f"Editor request timed out: {method}")
			if isinstance(message, BaseException):
				raise message
			if message.get("id") == self.identifier:
				assert "error" not in message, message
				return message["result"]

	def ready(self, uri, version, require_bindings=True):
		deadline = time.monotonic() + 20
		while time.monotonic() < deadline:
			status = self.call("rutile/indexStatus", {})
			assert status["analysisPhase"] != "failed", f"Editor analysis failed: {status}"
			indexed = {item["uri"]: item["indexedVersion"] for item in status["documents"]}
			if ((not require_bindings or status["bindingsCurrent"]) and status["indexedGeneration"] == status["generation"]
				and indexed.get(uri) == version):
				return status
			time.sleep(.02)
		raise AssertionError(f"Editor index did not become ready: {status}")


	def diagnostics(self, uri, version):
		matching = [item for item in self.notifications if item.get("uri") == uri and item.get("version") == version]
		assert matching, f"Missing diagnostics publication for version {version}"
		return matching[-1]["diagnostics"]

	def token_rows(self, uri, source):
		data = self.call("textDocument/semanticTokens/full", {"textDocument": {"uri": uri}})["data"]
		assert len(data) % 5 == 0, data
		line = character = 0
		previous_end = (0, 0)
		found = []
		lines = source.splitlines()
		for offset in range(0, len(data), 5):
			delta_line, delta_character, length, kind, _ = data[offset:offset + 5]
			line += delta_line
			character = delta_character if delta_line else character + delta_character
			assert length > 0 and 0 <= kind < len(self.legend), data[offset:offset + 5]
			assert previous_end <= (line, character), "Overlapping token ranges"
			encoded = lines[line].encode("utf-16-le")
			assert (character + length) * 2 <= len(encoded), (line, character, length)
			text = encoded[character * 2:(character + length) * 2].decode("utf-16-le")
			found.append((line, character, text, self.legend[kind]))
			previous_end = (line, character + length)
		return found

	def tokens(self, uri, source):
		lines = source.splitlines()
		found = [(text, kind) for _, _, text, kind in self.token_rows(uri, source)]
		for expected in [("answer", "function"), ("covered", "function"), ("string_view", "struct"),
			("// editor comment", "comment"),
			("/* block comment", "comment"), ("continued comment */", "comment"),
			('"if // text"', "string"), ("'x'", "string"), ("41", "number"),
			("40", "number"), ("+", "operator"), (">=", "operator"),
			("&&", "operator"), ("=", "operator"), ("fn", "keyword"),
			("var", "keyword"), ("true", "keyword"), ("if", "controlKeyword"),
			("return", "controlKeyword")]:
			assert expected in found, f"Missing server token {expected}: {found}"
		if lines[-1].startswith("// edit "):
			assert (lines[-1], "comment") in found, found


	def commented_binding(self, uri, source, require_bindings=True):
		rows = self.token_rows(uri, source)
		found = [(text, kind) for _, _, text, kind in rows]
		comment = "// fn covered()->i32 { return 7; };"
		line = next(index for index, text in enumerate(source.splitlines()) if comment in text)
		assert [(text, kind) for row, _, text, kind in rows if row == line] == [(comment, "comment")], rows
		expected_tokens = [("// pending semantic error", "comment")]
		if require_bindings:
			expected_tokens += [("answer", "function"), ("string_view", "struct")]
		for expected in expected_tokens:
			assert expected in found, f"Missing live token {expected}: {found}"


	def completion(self, uri, source, needle, prefix, trigger_kind=1):
		offset = source.index(needle) + len(prefix)
		line = source.count("\n", 0, offset)
		character = len(source[source.rfind("\n", 0, offset) + 1:offset].encode("utf-16-le")) // 2
		end = offset
		while end < len(source) and (source[end].isalnum() or source[end] == "_"):
			end += 1
		end_character = character + len(source[offset:end].encode("utf-16-le")) // 2
		items = self.call("textDocument/completion", {"textDocument": {"uri": uri},
			"position": {"line": line, "character": character}, "context": {"triggerKind": trigger_kind}})
		if isinstance(items, dict):
			items = items["items"]
		for item in items:
			assert item["filterText"] == item["label"], item
			assert "sortText" in item and "kind" in item, item
			assert item["textEdit"] == {"range": {"start": {"line": line, "character": character - len(prefix)},
				"end": {"line": line, "character": end_character}}, "newText": item["label"]}, item
		return sorted(items, key=lambda item: item["sortText"])

	def completion_scope(self, uri, source, needle, prefix, trigger_kind=1):
		items = self.completion(uri, source, needle, prefix, trigger_kind)
		names = [item["label"] for item in items]
		if not prefix:
			assert names and names[0] == "this", items
			for name in ["i32", "bool", "fn", "return"]:
				assert name in names and names.index(name) > names.index("complete_global"), items
				assert next(item for item in items if item["label"] == name)["kind"] == (7 if name in ["i32", "bool"] else 14), items
		for name in ["complete_inner", "complete_local", "complete_parameter", "complete_member", "complete_global"]:
			assert name in names, (name, items)
		assert names.index("complete_inner") < names.index("complete_local"), items
		assert names.index("complete_local") < names.index("complete_parameter") < names.index("complete_member") < names.index("complete_global"), items
		assert "complete_future" not in names and "complete_hidden" not in names, items
		shadowed = [item for item in items if item["label"] == "complete_shadow"]
		assert len(shadowed) == 1 and "i32" in shadowed[0]["detail"] and "i64" not in shadowed[0]["detail"], shadowed
		for item in items:
			if item["label"] in ["complete_inner", "complete_local", "complete_parameter", "complete_global"]:
				assert item["kind"] == 6, item
			if item["label"] == "complete_member":
				assert item["kind"] == 5, item

	def close(self):
		try:
			self.call("shutdown", {})
			self.send("exit", {})
		finally:
			self.input.close()
			self.pipe.close()


def run(executable):
	output = Path(__file__).resolve().parents[2] / "build/native-tests"
	output.mkdir(parents=True, exist_ok=True)
	with tempfile.TemporaryDirectory(prefix="language-reconnect-", dir=output) as temporary:
		root = Path(temporary).resolve()
		endpoint = endpoint_for(root) + "-rutilec"
		source = """// editor comment
/* block comment
continued comment */
namespace rt { struct string_view { char* bytes; usize length; }; };
fn covered()->i32 { return 7; };
fn answer()->i32 {
	var count = i32(41);
	char letter = 'x';
	var text = "if // text";
	bool enabled = true;
	covered();
	if (enabled && count >= 40) { return count + 1; };
	return 0;
};
"""

		completion_source = source + """
i32 complete_global;
i64 complete_shadow;
struct CompletionRecord {
	i32 complete_member;
	fn inspect(CompletionRecord& this, i32 complete_parameter)->i32 {
		var complete_local = i32(1);
		var complete_shadow = i32(2);
		{ var complete_hidden = i32(3); };
		{
			var complete_inner = i32(4);
			this.complete_member; // member cursor
			/* closed */complete_inner; // cursor
			var complete_future = i32(5);
		};
		return complete_local;
	};
};
"""

		file = root / "main.rt"
		file.write_text(source, encoding="utf-8")
		with (root / "compiler.log").open("wb") as log:
			process = subprocess.Popen([str(executable), "serve", endpoint], cwd=root,
				stdout=log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
			watchdog = threading.Timer(90, process.kill)
			watchdog.start()
			editor = None
			print(f"Compiler started: PID {process.pid}", flush=True)
			try:
				deadline = time.monotonic() + 10
				while True:
					assert process.poll() is None, "Compiler exited during startup"
					try:
						hello = json.loads(bounded_request(endpoint, "hello"))
						break
					except OSError:
						if time.monotonic() >= deadline:
							raise
						time.sleep(.02)
				assert hello["pid"] == process.pid, hello
				print("Compiler control connection ready", flush=True)
				manifest = {"sources": [{"group": "app.obj", "path": str(file), "kind": 0}]}
				configured = json.loads(bounded_request(endpoint, "sources " + json.dumps(manifest)))
				assert configured["accepted"] and configured["pid"] == process.pid, configured
				print("Project sources configured", flush=True)
				for session in range(2):
					print(f"Session {session + 1}: connecting", flush=True)
					editor = Editor(endpoint, root, custom_colors=session == 0)
					try:
						uri = file.as_uri()
						editor.send("textDocument/didOpen", {"textDocument": {"uri": uri,
							"languageId": "rutile", "version": 1, "text": source}})
						status = editor.ready(uri, 1)
						assert editor.diagnostics(uri, 1) == [], editor.diagnostics(uri, 1)
						editor.tokens(uri, source)
						print(f"Session {session + 1}: generation {status['generation']} indexed with server-owned tokens", flush=True)
						if session == 0:
							for version in range(2, 10):
								changed = source + f"// edit {version}\n"
								editor.send("textDocument/didChange", {"textDocument": {"uri": uri,
									"version": version}, "contentChanges": [{"text": changed}]})
								editor.ready(uri, version)
								editor.tokens(uri, changed)
							print("Session 1: eight edits indexed with server-owned tokens", flush=True)

							broken = source.replace("fn covered()->i32 { return 7; };", "// fn covered()->i32 { return 7; };") + "// pending semantic error\n"
							editor.send("textDocument/didChange", {"textDocument": {"uri": uri,
								"version": 10}, "contentChanges": [
								{"range": {"start": {"line": source.splitlines().index("fn covered()->i32 { return 7; };"), "character": 0},
									"end": {"line": source.splitlines().index("fn covered()->i32 { return 7; };"), "character": 0}}, "text": "// "},
								{"range": {"start": {"line": len(source.splitlines()), "character": 0},
									"end": {"line": len(source.splitlines()), "character": len("// edit 9")}}, "text": "// pending semantic error"}]})
							# Request before waiting for indexing: old bindings must never cover current comments.
							editor.commented_binding(uri, broken)
							editor.ready(uri, 10, require_bindings=False)
							assert any("covered" in item["message"] for item in editor.diagnostics(uri, 10)), editor.diagnostics(uri, 10)
							editor.commented_binding(uri, broken)
							# Full replacements intentionally invalidate prior bindings, but retain live lexical colors.
							replaced = broken + "// full replacement\n"
							editor.send("textDocument/didChange", {"textDocument": {"uri": uri,
								"version": 11}, "contentChanges": [{"text": replaced}]})
							editor.commented_binding(uri, replaced, require_bindings=False)
							editor.ready(uri, 11, require_bindings=False)
							editor.commented_binding(uri, replaced)
							editor.send("textDocument/didChange", {"textDocument": {"uri": uri,
								"version": 12}, "contentChanges": [{"text": source}]})
							editor.ready(uri, 12)
							assert editor.diagnostics(uri, 12) == [], editor.diagnostics(uri, 12)
							editor.tokens(uri, source)
							print("Session 1: live and settled semantic errors retain current comment colors", flush=True)

							editor.send("textDocument/didChange", {"textDocument": {"uri": uri,
								"version": 13}, "contentChanges": [{"text": completion_source}]})
							editor.ready(uri, 13)
							assert editor.diagnostics(uri, 13) == [], editor.diagnostics(uri, 13)
							editor.completion_scope(uri, completion_source, "complete_inner; // cursor", "")

							assert editor.completion(uri, completion_source, "member cursor", "mem") == []
							assert editor.completion(uri, completion_source, "continued comment */", "cont") == []
							assert editor.completion(uri, completion_source, "if // text", "if") == []

							incomplete = completion_source.replace("complete_inner; // cursor", "compl; // cursor")
							editor.send("textDocument/didChange", {"textDocument": {"uri": uri,
								"version": 14}, "contentChanges": [{"text": incomplete}]})
							editor.ready(uri, 14, require_bindings=False)
							editor.completion_scope(uri, incomplete, "compl; // cursor", "compl", trigger_kind=3)
							incomplete = completion_source.replace("this.complete_member; // member cursor", "this.comp; // member cursor")
							editor.send("textDocument/didChange", {"textDocument": {"uri": uri,
								"version": 15}, "contentChanges": [{"text": incomplete}]})
							editor.ready(uri, 15, require_bindings=False)
							members = editor.completion(uri, incomplete, "comp; // member cursor", "comp", trigger_kind=3)
							assert [item["label"] for item in members] == ["complete_member"], members
							bare = completion_source + "fn completion_blank()->i32 {\n\ti32 completion_local = 42;\n\t// blank insertion\n" + "\t\n" + "\treturn completion_local;\n};\n"
							editor.send("textDocument/didChange", {"textDocument": {"uri": uri,
								"version": 16}, "contentChanges": [{"text": bare}]})
							editor.ready(uri, 16)
							assert editor.diagnostics(uri, 16) == [], editor.diagnostics(uri, 16)

							items = editor.completion(uri, bare, "\n\treturn completion_local;", "")
							assert items and items[0]["label"] == "completion_local", ("valid blank line", items)

							point = bare.index("\n", bare.index("// blank insertion")) + 2
							line = bare.count("\n", 0, point)
							incomplete = bare[:point] + "c" + bare[point:]
							editor.send("textDocument/didChange", {"textDocument": {"uri": uri,
								"version": 17}, "contentChanges": [{"range": {"start": {"line": line, "character": 1},
									"end": {"line": line, "character": 1}}, "text": "c"}]})
							for settled in [False, True]:
								if settled:
									editor.ready(uri, 17, require_bindings=False)
									assert editor.diagnostics(uri, 17), "Bare identifier fixture must be incomplete"
								items = editor.completion(uri, incomplete, "c\n\treturn completion_local;", "c")
								assert items and items[0]["label"] == "completion_local", (settled, items)
							editor.send("textDocument/didChange", {"textDocument": {"uri": uri,
								"version": 18}, "contentChanges": [{"text": source}]})
							editor.ready(uri, 18)
							assert editor.diagnostics(uri, 18) == [], editor.diagnostics(uri, 18)
							print("Session 1: scoped completion ranking, shadowing, editable prefixes and member filtering PASS", flush=True)


					except BaseException:
						# The outer cleanup kills our compiler before closing a failed pipe.
						raise
					else:
						print(f"Session {session + 1}: closing gracefully", flush=True)
						editor.close()
						editor = None
						print(f"Session {session + 1}: closed", flush=True)
					assert process.poll() is None, "Compiler stopped when editor disconnected"
					assert json.loads(bounded_request(endpoint, "hello"))["pid"] == process.pid
				assert bounded_request(endpoint, "exit") == "Compiler stopped."
				assert process.wait(timeout=10) == 0
			finally:
				watchdog.cancel()
				if process.poll() is None:
					process.kill()
				process.wait(timeout=10)
				if editor is not None:
					editor.input.close()
					editor.pipe.close()
	print("Persistent compiler: edited session, graceful disconnect, same-PID reconnect and server-owned semantic/lexical colors PASS")


if __name__ == "__main__":
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("executable", type=Path)
	run(parser.parse_args().executable.resolve())
