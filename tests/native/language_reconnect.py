"""Verify editor indexing survives reconnects to the same persistent compiler."""

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
	def __init__(self, endpoint, root):
		self.pipe = open(endpoint + "-lsp", "r+b", buffering=0)
		self.input = os.fdopen(os.dup(self.pipe.fileno()), "rb", buffering=0)
		self.messages = queue.Queue()
		self.identifier = 0
		print("Editor pipe connected; initializing", flush=True)
		self.call("initialize", {"rootUri": root.as_uri(), "capabilities": {},
			"initializationOptions": {"rutileSemanticColors": True}})
		self.legend = self.call("rutile/semanticTokensLegend", {})["tokenTypes"]
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

	def ready(self, uri, version):
		deadline = time.monotonic() + 20
		while time.monotonic() < deadline:
			status = self.call("rutile/indexStatus", {})
			assert status["analysisPhase"] != "failed", f"Editor analysis failed: {status}"
			indexed = {item["uri"]: item["indexedVersion"] for item in status["documents"]}
			if (status["bindingsCurrent"] and status["indexedGeneration"] == status["generation"]
				and indexed.get(uri) == version):
				return status
			time.sleep(.02)
		raise AssertionError(f"Editor index did not become ready: {status}")

	def tokens(self, uri, source):
		data = self.call("textDocument/semanticTokens/full", {"textDocument": {"uri": uri}})["data"]
		line = character = 0
		found = []
		for offset in range(0, len(data), 5):
			delta_line, delta_character, length, kind, _ = data[offset:offset + 5]
			line += delta_line
			character = delta_character if delta_line else character + delta_character
			found.append((source.splitlines()[line][character:character + length], self.legend[kind]))
		assert ("answer", "function") in found, f"Missing function token: {found}"

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
		source = "fn answer()->i32 { return 42; };\n"
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
					editor = Editor(endpoint, root)
					try:
						uri = file.as_uri()
						editor.send("textDocument/didOpen", {"textDocument": {"uri": uri,
							"languageId": "rutile", "version": 1, "text": source}})
						status = editor.ready(uri, 1)
						editor.tokens(uri, source)
						print(f"Session {session + 1}: generation {status['generation']} indexed with function tokens", flush=True)
						if session == 0:
							for version in range(2, 10):
								changed = source + f"// edit {version}\n"
								editor.send("textDocument/didChange", {"textDocument": {"uri": uri,
									"version": version}, "contentChanges": [{"text": changed}]})
								editor.ready(uri, version)
								editor.tokens(uri, changed)
							print("Session 1: eight edits indexed with function tokens", flush=True)
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
	print("Persistent compiler: edited session, graceful disconnect, same-PID reconnect and function tokens PASS")


if __name__ == "__main__":
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("executable", type=Path)
	run(parser.parse_args().executable.resolve())
