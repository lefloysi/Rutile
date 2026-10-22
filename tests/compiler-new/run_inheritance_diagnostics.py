"""Verify base and virtual dispatch failures name their declarations and actual types."""

import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
CASES = [
	("union-base", "struct Base {}; union Choice : [Base] {};", [r"union 'Choice' cannot contain a base subobject"]),
	("multiple-bases", "struct First {}; struct Second {}; struct Child : [First, Second] {};", [r"record 'Child' declares 2 base types", r"multiple base analysis is not implemented"]),
	("primitive-base", "struct Child : [i32] {};", [r"invalid base type 'i32' for record 'Child': base requires a record type"]),
	("const-base", "struct Base {}; struct Child : [const Base] {};", [r"invalid base type 'const Base' for record 'Child': base type shall be unqualified"]),
	("overlapping-base", "union Choice {}; struct Child : [Choice] {};", [r"invalid base type 'Choice' for record 'Child': base type shall not be a union"]),
	("unknown-base", "struct Child : [Missing] {};", [r"unknown type name 'Missing'"]),
	("incomplete-base", "struct Base; struct Child : [Base] {};", [r"base requires a complete record definition; record 'Child' names base 'Base'", r"note: base record is declared here"]),
	("cyclic-base", "struct First; struct Second : [First] {}; struct First : [Second] {};", [r"base relation contains a cycle; record '.*' names base '.*'", r"note: base record is declared here"]),
	("receiver-missing", "struct Owner { fn inspect() -> i32 : [virtual]; };", [r"declaration 'Owner::inspect': virtual operation requires an explicit ordinary class receiver named this", r"declared signature is '\(\) -> i32'", r"no receiver parameter was declared"]),
	("receiver-name", "struct Owner { fn inspect(const Owner& self) -> i32 : [virtual]; };", [r"declaration 'Owner::inspect': virtual operation requires an explicit ordinary class receiver named this", "first\\ parameter\\ is\\ 'Owner\\&\\ self'"]),
	("receiver-type", "struct Owner { fn inspect(Owner this) -> i32 : [virtual]; };", [r"declaration 'Owner::inspect': virtual operation requires an explicit ordinary class receiver named this", r"first parameter is 'Owner this'"]),
	("template-virtual", "struct Owner { template<typename T> fn inspect(const Owner& this, T value) -> i32 : [virtual]; };", [r"attribute 'virtual' on function template 'inspect'", r"virtual function templates.*not implemented"]),
	("final-override", "struct Base { fn inspect(const Base& this) -> i32 : [virtual, final]; }; struct Child : [Base] { fn inspect(const Child& this) -> i32 : [override]; };", [r"final virtual operation cannot be overridden; function 'Child::inspect'", r"note: candidate 'Base::inspect' has type"]),
	("override-required", "struct Base { fn inspect(const Base& this) -> i32 : [virtual]; }; struct Child : [Base] { fn inspect(const Child& this) -> i32; };", [r"override of an inherited virtual operation requires override or final; function 'Child::inspect'", r"note: candidate 'Base::inspect'"]),
	("override-result", "struct Base { fn inspect(const Base& this) -> i32 : [virtual]; }; struct Child : [Base] { fn inspect(const Child& this) -> bool : [override]; };", [r"virtual override has an incompatible result type; function 'Child::inspect'", r"with result 'bool'; inherited result is 'i32'", r"note: candidate 'Base::inspect'"]),
	("override-unmatched", "struct Owner { fn inspect(const Owner& this) -> i32 : [override]; };", ["override\\ on\\ function\\ 'Owner::inspect'\\ of\\ type\\ '\\(Owner\\&\\)\\ \\->\\ i32'\\ requires\\ a\\ matching\\ inherited\\ virtual\\ operation"]),
	("final-unmatched", "struct Owner { fn inspect(const Owner& this) -> i32 : [final]; };", [r"final without virtual on function 'Owner::inspect'", r"requires a matching inherited virtual operation"]),
	("union-virtual", "union Choice { fn inspect(const Choice& this) -> i32 : [virtual]; };", [r"union 'Choice' cannot contain virtual operations"]),
]


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", default=".kiln/build/release/target-727574696c65632e657865/rutilec.exe")
	options = parser.parse_args()
	directory = ROOT / "build/compiler-new/inheritance-diagnostics"
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
