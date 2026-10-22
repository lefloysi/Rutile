"""Check construction, explicit lifetime calls, and variadic call failures."""

import argparse
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[2]
CASES = [
	("object-type", "fn main() -> i32 { void(); return 0; };",
		[r"construction requires an object type; requested type is 'void'"]),
	("scalar-count", "fn main() -> i32 { return i32(1, true); };",
		[r"scalar construction of 'i32' accepts at most one argument; received 2 with types \(i32, bool\)"]),
	("aggregate-count", "struct Box { i32 value; }; fn main() -> i32 { Box box = Box(1, true); return 0; };",
		[r"too many aggregate initializer arguments for 'Box': expected at most 1, received 2", r"types \(i32, bool\)"]),
	("arrow-receiver", "fn main() -> i32 { i32 value = 1; return value->read(); };",
		[r"arrow method access to 'read' requires a pointer; receiver has type 'i32'"]),
	("method-receiver", "fn main() -> i32 { i32 value = 1; return value.read(); };",
		[r"method 'read' requires a class or enum object; receiver has type 'i32'"]),
	("construct-access", "fn main() -> i32 { i32 value = 1; value.i32(2); return 0; };",
		[r"explicit construction of 'i32' requires mutable access", r"object was not accessed through '@'"]),
	("destroy-const", "fn main() -> i32 { const i32 value = 1; @value.~i32(); return 0; };",
		[r"explicit destruction of 'const i32' requires mutable access; the object is const"]),
	("destroy-count", "fn main() -> i32 { i32 value = 1; @value.~i32(1); return 0; };",
		[r"destructor for 'i32' accepts no arguments after its receiver; received 1"]),
	("destroy-type", "fn main() -> i32 { i32 value = 1; @value.~u32(); return 0; };",
		[r"destructor type 'u32' does not match its object of type 'i32'"]),
	("destroy-unknown", "fn main() -> i32 { i32 value = 1; @value.~Missing(); return 0; };",
		[r"unknown type name 'Missing'"]),
	("construct-type", "fn main() -> i32 { i32 value = 1; @value.u32(); return 0; };",
		[r"explicit construction type 'u32' does not match its destination of type 'i32'"]),
	("variadic-type", "struct Box {}; fn take(...); fn main() -> i32 { Box box; take(box); return 0; };",
		[r"variadic argument 1 requires a scalar value; received 'Box'", r"callable type '\(\.\.\.\) -> void'"]),
]


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", default=".kiln/build/release/target-727574696c65632e657865/rutilec.exe")
	options = parser.parse_args()
	output = ROOT / "build/compiler-new/call-diagnostics"
	output.mkdir(parents=True, exist_ok=True)
	for name, source, patterns in CASES:
		path = output / (name + ".rt")
		path.write_text(source + "\n", encoding="utf-8")
		result = subprocess.run([str(ROOT / options.compiler), str(path), "--output", str(output / (name + ".obj"))], capture_output=True, text=True, timeout=30)
		message = result.stdout + result.stderr
		if result.returncode != 1 or "internal compiler error" in message or len(re.findall(r": error:", message)) != 1 or any(not re.search(pattern, message) for pattern in patterns):
			raise RuntimeError(f"{name}: unexpected diagnostic (exit {result.returncode})\n{message}")
		print(name + ": PASS")


if __name__ == "__main__":
	main()
