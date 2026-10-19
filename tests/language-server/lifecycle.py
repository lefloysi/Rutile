"""Verify the IDE owns the same project Kiln that serves command-line requests."""

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from protocol import Client


def main(executable):
	output = Path(__file__).resolve().parents[2] / "build" / "language-server-tests"
	output.mkdir(parents=True, exist_ok=True)
	with tempfile.TemporaryDirectory(prefix="managed kiln ", dir=output) as directory:
		root = Path(directory)
		client = Client(executable, root)
		try:
			client.request("initialize", {"rootUri": root.as_uri(), "capabilities": {}})
			assert (root / ".rt" / "connection").read_text().strip()
			status = subprocess.run([str(executable), "status"], cwd=root,
				capture_output=True, text=True, timeout=5)
			assert status.returncode == 0, status.stdout + status.stderr
			assert "IDE" in status.stdout, status.stdout
			exit_request = subprocess.run([str(executable), "exit"], cwd=root,
				capture_output=True, text=True, timeout=5)
			assert exit_request.returncode == 1
			assert "managed by the IDE" in exit_request.stderr, exit_request.stderr
			duplicate = subprocess.run([str(executable), "language-server", "--stdio"],
				cwd=root, input=b"", capture_output=True, timeout=5)
			assert duplicate.returncode == 1
			assert b"already has a Kiln owner" in duplicate.stderr, duplicate.stderr
			assert not duplicate.stdout
			client.request("rutile/indexStatus")
			client.close()
		finally:
			if client.process.poll() is None:
				client.process.kill()
				client.process.wait(timeout=5)
		# The IDE connection's shutdown releases the project endpoint and process lock.
		reopened = Client(executable, root)
		try:
			reopened.request("initialize", {"rootUri": root.as_uri(), "capabilities": {}})
			reopened.close()
		finally:
			if reopened.process.poll() is None:
				reopened.process.kill()
				reopened.process.wait(timeout=5)
	print("IDE-owned Kiln, shared project endpoint, duplicate rejection and shutdown: PASS")


if __name__ == "__main__":
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--server", type=Path, default=Path("../Kiln/build/bootstrap/bin/kiln.exe"))
	main(parser.parse_args().server.resolve())
