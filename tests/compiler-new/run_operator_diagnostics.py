"""Verify operator failures preserve their cause and identify operands and candidates."""

import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
CASES = [
	("unavailable", "struct Value {}; fn operator+(const Value& first, var i32* second) -> i32; fn main() -> i32 { Value value; return value + true; };",
		[r"no applicable operator '\+' for operands \(Value, bool\)", "note:\\ candidate\\ 'operator\\+'\\ has\\ type\\ '\\(Value\\&,\\ var\\ i32\\*\\)\\ \\->\\ i32'"]),
	("ambiguous", "struct Value {}; fn operator+(const Value& first, var i32* second) -> i32; fn operator+(const Value& first, var bool* second) -> i32; fn main() -> i32 { Value value; return value + nullptr; };",
		["ambiguous\\ call\\ to\\ 'operator\\+'\\ with\\ arguments\\ \\(Value,\\ null\\*\\)", r"equally ranked candidate", r"i32\*", r"bool\*"]),
	("deleted", "struct Value {}; fn operator+(const Value& first, i32 second) -> i32 : [delete]; fn main() -> i32 { Value value; return value + 1; };",
		[r"selected function 'operator\+'", r"is deleted", r"note: function is declared deleted here"]),
	("inaccessible", "struct Value { private: fn operator+(const Value& this, i32 second) -> i32; }; fn main() -> i32 { Value value; return value + 1; };",
		["selected\\ operator\\ 'Value::operator\\+'\\ of\\ type\\ '\\(Value\\&,\\ i32\\)\\ \\->\\ i32'\\ is\\ not\\ accessible\\ from\\ this\\ scope", r"note: operator is declared here"]),
]


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", default=".kiln/build/release/target-727574696c65632e657865/rutilec.exe")
	options = parser.parse_args()
	directory = ROOT / "build/compiler-new/operator-diagnostics"
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
