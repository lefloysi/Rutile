"""Exercise the persistent compiler IPC, mapped input, and failure recovery on Windows."""
import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import subprocess
import sys
import threading
import time

sys.dont_write_bytecode = True


class FileInformation(ctypes.Structure):
	_fields_ = [("attributes", wintypes.DWORD), ("creation", wintypes.FILETIME),
				("access", wintypes.FILETIME), ("write", wintypes.FILETIME),
				("volume", wintypes.DWORD), ("size_high", wintypes.DWORD),
				("size_low", wintypes.DWORD), ("links", wintypes.DWORD),
				("index_high", wintypes.DWORD), ("index_low", wintypes.DWORD)]


def endpoint_for(directory):
	kernel = ctypes.WinDLL("kernel32", use_last_error=True)
	kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
								  ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD,
								  wintypes.HANDLE]
	kernel.CreateFileW.restype = wintypes.HANDLE
	kernel.GetFileInformationByHandle.argtypes = [wintypes.HANDLE, ctypes.POINTER(FileInformation)]
	kernel.CloseHandle.argtypes = [wintypes.HANDLE]
	handle = kernel.CreateFileW(str(directory), 0x80, 7, None, 3, 0x02000000, None)
	if handle == ctypes.c_void_p(-1).value:
		raise ctypes.WinError(ctypes.get_last_error())
	try:
		information = FileInformation()
		if not kernel.GetFileInformationByHandle(handle, ctypes.byref(information)):
			raise ctypes.WinError(ctypes.get_last_error())
		return ("\\\\.\\pipe\\kiln-" + f"{information.volume:08x}-"
				+ f"{information.index_high:08x}{information.index_low:08x}")
	finally:
		kernel.CloseHandle(handle)


def request(endpoint, text):
	with open(endpoint, "r+b", buffering=0) as pipe:
		data = (text + "\n").encode("utf-8")
		offset = 0
		while offset < len(data):
			offset += pipe.write(data[offset:])
		response = bytearray()
		while True:
			value = pipe.read(1)
			if not value:
				raise RuntimeError("Compiler disconnected without a response")
			if value == b"\n":
				return response.decode("utf-8")
			response += value


def run(compiler, directory):
	directory.mkdir(parents=True, exist_ok=True)
	connection_directory = directory / ".kiln" / "rutile"
	connection_directory.mkdir(parents=True, exist_ok=True)
	endpoint = endpoint_for(connection_directory)
	log = open(directory / "compiler-service.log", "wb")
	process = subprocess.Popen([str(compiler), "serve", endpoint], cwd=directory,
							   stdout=log, stderr=subprocess.STDOUT,
							   creationflags=subprocess.CREATE_NO_WINDOW)
	watchdog = threading.Timer(45, process.kill)
	watchdog.start()
	try:
		deadline = time.monotonic() + 10
		while True:
			if process.poll() is not None:
				raise RuntimeError("Compiler exited during startup")
			try:
				hello = json.loads(request(endpoint, "hello"))
				break
			except OSError:
				if time.monotonic() > deadline:
					raise
				time.sleep(0.02)
		assert hello["pid"] == process.pid and hello["compilations"] == 0
		duplicate = subprocess.run([str(compiler), "serve", endpoint], cwd=directory,
								   stdout=subprocess.PIPE, stderr=subprocess.PIPE,
								   timeout=5, creationflags=subprocess.CREATE_NO_WINDOW)
		assert duplicate.returncode == 1 and b"already running" in duplicate.stderr
		rejected = subprocess.run([str(compiler), "serve", endpoint + "-wrong"], cwd=directory,
								  stdout=subprocess.PIPE, stderr=subprocess.PIPE,
								  timeout=5, creationflags=subprocess.CREATE_NO_WINDOW)
		assert rejected.returncode == 1 and b"does not belong" in rejected.stderr
		first = directory / "first source.rt"
		second = directory / "second source.rt"
		originals = [b"// UTF-8 source: \xc3\xa4\nfn answer() -> i32 { return value(); };",
					 b"fn value() -> i32 { return 42; };"]
		first.write_bytes(originals[0])
		second.write_bytes(originals[1])
		output = directory / "output.obj"
		payload = {"request_id": "test-valid", "sources": [str(first), str(second)],
				   "output": str(output), "optimization": 0, "kind": "object"}

		def compile_request(value):
			result = json.loads(request(endpoint, "compile " + json.dumps(value)))
			assert result["pid"] == process.pid
			return result

		def check_bundle(result, texts):
			source_map = json.loads(Path(result["source_map"]).read_text(encoding="utf-8"))
			bundle = Path(result["bundle"]).read_bytes()
			assert source_map["bundle"] == result["bundle"]
			assert len(source_map["sources"]) == 2
			for entry, path, text in zip(source_map["sources"], [first, second], texts):
				assert entry["path"] == str(path)
				assert bundle[entry["offset"]:entry["offset"] + entry["length"]] == text
				assert entry["length"] == len(text)

		result = compile_request(payload)
		assert result["succeeded"] and result["compilations"] == 1, result
		assert output.stat().st_size > 0
		check_bundle(result, originals)
		progress = json.loads((connection_directory / "progress.json").read_text())
		assert progress["request_id"] == payload["request_id"]
		assert progress["state"] == "succeeded" and progress["pid"] == process.pid
		events = [json.loads(line) for line in (connection_directory / "progress.events.jsonl").read_text().splitlines()]
		assert any(event["phase"] == "functions" and event["detail"].endswith(f"{first} :: answer") for event in events)
		assert any(event["phase"] == "functions" and event["detail"].endswith(f"{second} :: value") for event in events)

		# Incremental requests replace only changed files in the persistent source snapshot.
		first.unlink()
		updated_second = b"fn value() -> i32 { return 43; };"
		second.write_bytes(updated_second)
		incremental = dict(payload, request_id="test-incremental", incremental=True,
					   changed_sources=[str(second)])
		result = compile_request(incremental)
		assert result["succeeded"] and result["compilations"] == 2, result
		check_bundle(result, [originals[0], updated_second])
		events = [json.loads(line) for line in (connection_directory / "progress.events.jsonl").read_text().splitlines()]
		assert any(event["phase"] == "changed-source" and event["detail"] == str(second) for event in events)
		first.write_bytes(originals[0])
		second.write_bytes(originals[1])

		broken = b"// preserve original line\nfn value() -> i32 { return missing(); };"
		overlays = directory / "overlays.json"
		overlays.write_text(json.dumps({str(second): broken.decode("utf-8")}), encoding="utf-8")
		invalid_payload = dict(payload, request_id="test-invalid", overlays_path=str(overlays))
		result = compile_request(invalid_payload)
		assert not result["succeeded"] and result["compilations"] == 3, result
		assert str(second) + ":2:" in result["diagnostics"], result
		assert "missing" in result["diagnostics"]
		assert second.read_bytes() == originals[1]
		check_bundle(result, [originals[0], broken])
		progress = json.loads((connection_directory / "progress.json").read_text())
		assert progress["request_id"] == "test-invalid" and progress["state"] == "failed"

		# Input errors and malformed messages must leave the same service available.
		malformed = json.loads(request(endpoint, "compile {"))
		assert not malformed["succeeded"] and malformed["pid"] == process.pid
		missing = compile_request(dict(payload, request_id="test-missing", sources=[str(directory / "absent.rt")]))
		assert not missing["succeeded"] and "Cannot read source input" in missing["diagnostics"]
		# Protect all three output paths.
		for suffix in ["", ".sources.rt", ".sources.json"]:
			protected_output = directory / ("protected" + suffix.replace(".", "-"))
			protected_source = Path(str(protected_output) + suffix)
			protected_source.write_bytes(originals[0])
			protected = compile_request(dict(payload, request_id="test-overwrite", output=str(protected_output),
											 sources=[str(protected_source), str(second)]))
			assert not protected["succeeded"] and "overwrite" in protected["diagnostics"], protected
			assert protected_source.read_bytes() == originals[0]

		result = compile_request(dict(payload, request_id="test-recovered", optimization=2))
		assert result["succeeded"] and result["compilations"] == 4, result
		check_bundle(result, originals)
		hello = json.loads(request(endpoint, "hello"))
		assert hello["pid"] == process.pid and hello["compilations"] == 4
		assert request(endpoint, "exit") == "Compiler stopped."
		assert process.wait(timeout=5) == 0
		print("compiler-service: PASS (same PID; failed build recovery; overlay locations; bundle mapping; source protection)")
	finally:
		watchdog.cancel()
		if process.poll() is None:
			process.kill()
			process.wait(timeout=5)
		log.close()


if __name__ == "__main__":
	root = Path(__file__).resolve().parents[2]
	compiler = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else root.parent / "Kiln/build/bootstrap/bin/rutilec.exe"
	run(compiler, root / "build/native-tests/compiler-service/project")
