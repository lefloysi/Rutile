"""Check lifetime diagnostics identify the affected binding without changing tracking rules."""

import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
CASES = [
	("relocated", "fn main() -> i32 { i32 parcel = 42; $parcel; return parcel; };", [r"object has been relocated; object 'main::parcel' has type 'i32'", r"note: object is declared here"]),
	("destroyed", "fn main() -> i32 { i32 parcel = 42; @parcel.~i32(); return parcel; };", [r"object has been destroyed; object 'main::parcel' has type 'i32'", r"note: object is declared here"]),
	("borrowed-owner", "fn main() -> i32 { i32 cargo = 42; const i32& borrowed = cargo; $cargo; return borrowed; };", [r"object has been relocated; object 'main::cargo' has type 'i32'", r"note: object is declared here"]),
	("branch-merge", "fn inspect(bool condition) -> i32 { i32 parcel = 42; if (condition) { $parcel; }; return parcel; }; fn main() -> i32 { return 0; };", [r"object lifetime differs across reachable control-flow paths; object 'inspect::parcel' has type 'i32'", r"note: object is declared here"]),
	("loop-backedge", "fn consume(i32 input); fn main() -> i32 { i32 packet = 42; while (true) { consume($packet); }; return 0; };", [r"inconsistent object lifetimes at reachable loop backedge; object 'main::packet' has type 'i32'", r"note: object is declared here"]),
	("sequence-local", "fn take(const i32& borrowed); fn main() -> i32 { take({ i32 enclosed = 42; @borrowed = enclosed; }); return 0; };", [r"sequence result refers to local storage; object 'main::enclosed' has type 'i32'", r"note: object is declared here"]),
]


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", default="build/release/bin/rutile-selfhost.exe")
	options = parser.parse_args()
	directory = ROOT / "build/compiler-new/lifetime-diagnostics"
	directory.mkdir(parents=True, exist_ok=True)
	for name, source, patterns in CASES:
		path = directory / (name + ".rt")
		path.write_text(source + "\n", encoding="utf-8")
		result = subprocess.run([str(ROOT / options.compiler), str(path), "--output", str(directory / (name + ".obj"))], capture_output=True, text=True, timeout=30)
		output = result.stdout + result.stderr
		if result.returncode != 1 or "internal compiler error" in output or len(re.findall(r": error:", output)) != 1 or any(not re.search(pattern, output) for pattern in patterns):
			raise RuntimeError(f"{name}: unexpected diagnostic (exit {result.returncode})\n{output}")
		print(name + ": PASS")


if __name__ == "__main__":
	main()
