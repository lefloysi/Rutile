"""Check switch failures retain actual types, values, and prior label locations."""

import argparse
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[2]
CASES = [
	("condition-type", "fn main() -> i32 { switch (1.5) {}; return 0; };",
		[r"switch condition requires an integer or enumeration value; received 'f64'"]),
	("case-context", "fn main() -> i32 { case 1: return 0; };",
		[r"'case' label requires an enclosing switch"]),
	("default-context", "fn main() -> i32 { default: return 0; };",
		[r"'default' label requires an enclosing switch"]),
	("default-duplicate", "fn main() -> i32 { switch (1) { default: ; default: ; }; return 0; };",
		[r"switch contains more than one default label", r"note: previous default label is here"]),
	("case-unknown", "fn main() -> i32 { switch (1) { case missing: ; }; return 0; };",
		[r"unknown identifier 'missing'"]),
	("case-runtime", "fn main() -> i32 { i32 label = 1; switch (1) { case label: ; }; return 0; };",
		[r"case label requires an integer constant expression for switch type 'i32'", r"'label' of type 'i32' is not constant"]),
	("case-duplicate", "fn main() -> i32 { switch (1) { case 1: ; case 2 - 1: ; }; return 0; };",
		[r"duplicate switch case value 1 for switch type 'i32'", r"note: previous case label is here"]),
	("case-negative", "fn main() -> i32 { switch (-1) { case -1: ; case -1: ; }; return 0; };",
		[r"duplicate switch case value -1 for switch type 'i32'", r"note: previous case label is here"]),
	("case-enum", "enum Colour : [i32] { red = 1; }; fn main() -> i32 { switch (Colour::red) { case 1: ; case 1: ; }; return 0; };",
		[r"duplicate switch case value 1 for switch type 'Colour'", r"note: previous case label is here"]),
]


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", default=".kiln/build/release/target-636f6d70696c65722e657865/compiler.exe")
	options = parser.parse_args()
	output = ROOT / "build/compiler-new/switch-diagnostics"
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
