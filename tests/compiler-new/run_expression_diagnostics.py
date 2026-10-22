"""Check expression diagnostics identify the failing operand and preserve prior failures."""

import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
CASES = [
	("place", "fn main() -> i32 { &42; return 0; };", [r"operator requires a place", r"operator '&' received a consumable of type 'i32'"]),
	("dereference", "fn main() -> i32 { *42; return 0; };", [r"dereference requires a pointer to an object", r"operator '\*'.*type 'i32'"]),
	("complement", "fn main() -> i32 { ~f32(1); return 0; };", [r"bitwise complement requires an integer", r"operator '~'.*type 'f32'"]),
	("unary-arithmetic", "fn main() -> i32 { +nullptr; return 0; };", [r"unary arithmetic requires a numeric value", "operator\\ '\\+'\\ received\\ a\\ consumable\\ of\\ type\\ 'null\\*'"]),
	("assignment", "fn main() -> i32 { i32 value; value = 1; return 0; };", [r"assignment requires explicit mutable access to a place", r"operator '=' received a place of type 'i32'"]),
	("pointer-comparison", "fn main() -> i32 { var i32* first; var bool* second; first == second; return 0; };", [r"pointer comparison requires compatible pointer types", "operand\\ types\\ 'var\\ i32\\*'\\ and\\ 'var\\ bool\\*'"]),
	("pointer-offset", "fn main() -> i32 { var void* pointer; pointer + 1; return 0; };", [r"pointer offset requires an integer and object element type", "operand\\ types\\ 'var\\ void\\*'\\ and\\ 'i32'"]),
	("bitwise", "fn main() -> i32 { f32(1) & f32(2); return 0; };", [r"bitwise arithmetic requires integers", r"operand types 'f32' and 'f32'"]),
	("cast", "fn main() -> i32 { static_cast<var i32*>(i32(1)); return 0; };", ["cast\\ 'static_cast'\\ does\\ not\\ permit\\ conversion\\ from\\ 'i32'\\ to\\ 'var\\ i32\\*'"]),
	("reference-cast", "fn main() -> i32 { static_cast<var i32&>(42); return 0; };", ["reference\\ cast\\ 'static_cast'\\ to\\ 'var\\ i32\\&'\\ requires\\ a\\ place", r"consumable of type 'i32'"]),
	("index-base", "fn main() -> i32 { i32 value; return value[0]; };", [r"indexing requires an array or pointer", r"type 'i32'"]),
	("index-type", "fn main() -> i32 { var i32* pointer; return pointer[f32(1)]; };", [r"index requires an integer", "operand\\ types\\ 'var\\ i32\\*'\\ and\\ 'f32'"]),
	("index-prior-error", "fn main() -> i32 { var i32* pointer; return pointer[missing]; };", [r"unknown identifier 'missing'"]),
	("member-base", "fn main() -> i32 { i32 value; return value.field; };", [r"member access for 'field' requires a class or enum object; received 'i32'"]),
	("member-unknown", "struct Value {}; fn main() -> i32 { Value value; return value.missing; };", [r"unknown data member 'missing' in type 'Value'"]),
	("member-kind", "struct Value { fn method(const Value& this) -> i32; }; fn main() -> i32 { Value value; return value.method; };", [r"member 'Value::method' does not denote a data member", r"note: member is declared here"]),
	("member-private", "struct Value { private: i32 hidden; }; fn main() -> i32 { Value value; return value.hidden; };", [r"data member 'Value::hidden' of type 'i32' is not accessible from this scope", r"note: data member is declared here"]),
	("cast-deleted", "struct Value {}; fn operator i32(const Value& this) -> i32 : [delete]; fn main() -> i32 { Value value; return static_cast<i32>(value); };", [r"selected function 'operator i32'.*is deleted", r"note: function is declared deleted here"]),
	("cast-private", "struct Value { private: fn operator i32(const Value& this) -> i32; }; fn main() -> i32 { Value value; return static_cast<i32>(value); };", ["selected\\ conversion\\ 'Value::operator\\ i32'\\ of\\ type\\ '\\(Value\\&\\)\\ \\->\\ i32'\\ is\\ not\\ accessible", r"note: conversion is declared here"]),
]


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", default=".kiln/build/release/target-727574696c65632e657865/rutilec.exe")
	options = parser.parse_args()
	directory = ROOT / "build/compiler-new/expression-diagnostics"
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
