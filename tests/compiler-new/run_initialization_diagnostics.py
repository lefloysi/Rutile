"""Verify initialization and sizeof failures identify types and selected declarations."""

import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
CASES = [
	("sizeof-void", "fn main() -> i32 { sizeof(void); return 0; };", [r"sizeof requires a complete object storage type; received 'void'"]),
	("sizeof-null", "fn main() -> i32 { sizeof(null); return 0; };", [r"sizeof requires a complete object storage type; received 'null'"]),
	("sizeof-incomplete", "struct Forward; fn main() -> i32 { sizeof(Forward); return 0; };", [r"sizeof requires a complete object storage type; received 'Forward'"]),
	("member-default-deleted", "struct Component { fn Component(var Component& this) : [delete]; }; struct Outer { Component member; }; fn main() -> i32 { Outer value; return 0; };", [r"selected function 'Component::Component'.*is deleted", r"note: function is declared deleted here"]),
	("member-default-unavailable", "struct Component { fn Component(var Component& this, i32 value); }; struct Outer { Component member; }; fn main() -> i32 { Outer value; return 0; };", [r"default initialization of 'Component' has no applicable constructor for an empty argument list", r"note: candidate 'Component::Component' has type"]),
	("union-initializer", "union Choice { i32 selected = 42; }; fn main() -> i32 { return 0; };", [r"default initializer for union member 'Choice::selected' of type 'i32' is not implemented", r"note: union member is declared here"]),
	("reference-member-consumable", "struct Owner { const i32& reference = 42; }; fn main() -> i32 { Owner value; return 0; };", ["initializer\\ for\\ reference\\ member\\ 'Owner::reference'\\ of\\ type\\ 'i32\\&'\\ requires\\ a\\ live\\ place", r"consumable of type 'i32'", r"note: reference member is declared here"]),
	("copy-member-deleted", "struct Component { fn Component(var Component& this) : [default]; fn Component(var Component& this, const Component& other) : [delete]; }; struct Outer { fn Outer(var Outer& this) : [default]; fn Outer(var Outer& this, const Outer& other) : [default]; Component member; }; fn main() -> i32 { Outer first; Outer second := first; return 0; };", [r"selected function 'Component::Component'.*is deleted", r"note: function is declared deleted here"]),
	("assignment-const-component", "struct Value { fn operator=(var Value& this, const Value& other) -> var Value& : [default]; const i32 fixed; }; fn main() -> i32 { Value first; Value second; @first = second; return 0; };", [r"generated assignment cannot replace component of type 'const i32' because it is const-qualified"]),
]


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", default=".kiln/build/release/target-636f6d70696c65722e657865/compiler.exe")
	options = parser.parse_args()
	directory = ROOT / "build/compiler-new/initialization-diagnostics"
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
