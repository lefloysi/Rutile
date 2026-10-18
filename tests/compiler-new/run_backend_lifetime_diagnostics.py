"""Verify native construction and destruction limits identify actual target types."""

import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
CASES = [
	("array-arguments", "fn main() -> i32 { var i32* values = new var i32[2](42); return 0; };", [r"array constructor argument evaluation is not specified for element type 'i32' with arguments \(i32\)"]),
	("array-delete-layout", "fn main() -> i32 { var void* values; delete[] values; return 0; };", [r"array deallocation requires a supported object layout for element type 'void'"]),
	("base-aggregate", "struct Base { i32 first; }; struct Child : [Base] { i32 second; }; fn main() -> i32 { Child value = Child(42); return 0; };", [r"base aggregate argument initialization is not implemented for type 'Child' with argument types \(i32\)"]),
	("union-aggregate", "union Choice { i32 first; i32 second; }; fn main() -> i32 { Choice value = Choice(42); return 0; };", [r"union aggregate argument selection is not implemented for type 'Choice' with argument types \(i32\)"]),
	("reference-field", "struct Owner { var i32& borrowed; }; fn main() -> i32 { Owner value; return 0; };", ["reference\\ member\\ 'Owner::borrowed'\\ of\\ type\\ 'var\\ i32\\&'\\ requires\\ an\\ explicit\\ initializer"]),
	("union-destructor", "struct Payload { fn ~Payload(var Payload& this); }; fn Payload::~Payload(var Payload& this) {}; union Choice { Payload owned; }; fn main() -> i32 { Choice value; return 0; };", [r"union 'Choice' requires an explicit destructor because member 'Choice::owned' of type 'Payload' requires destruction", r"note: member requiring destruction is declared here"]),
]


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", default="build/release/bin/rutile-selfhost.exe")
	options = parser.parse_args()
	directory = ROOT / "build/compiler-new/backend-lifetime-diagnostics"
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
