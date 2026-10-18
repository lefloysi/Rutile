"""Check declaration-scope failures identify names, categories and previous source sites."""

import argparse
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[2]
CASES = [
	("cyclic-alias", "using First = Second; using Second = First;", [r"cyclic entity definition for type alias '(First|Second)'"]),
	("enumerator-runtime", "i32 runtime; enum Colour : [i32] { red = runtime; };", [r"enumerator 'Colour::red' requires an integer constant of underlying type 'i32'", r"initializer has type 'i32' and is not constant"]),
	("enumerator-unknown", "enum Colour : [i32] { red = missing; };", [r"unknown identifier 'missing'"]),
	("module-access-label", "private:", [r"access label 'private' requires class scope; current scope is module scope"]),
	("namespace-access-label", "namespace tools { protected: };", [r"access label 'protected' requires class scope; current scope is namespace 'tools'"]),
	("module-expression", "42;", [r"declaration required at module or record scope; source begins with '42'"]),
	("namespace-expression", "namespace tools { 42; };", [r"declaration required at module or record scope; source begins with '42' in namespace tools"]),
	("missing-qualified-owner", "fn Missing::work();", [r"cannot declare 'Missing::work': owner 'Missing' is not declared"]),
	("scalar-qualified-owner", "i32 owner; fn owner::work();", [r"cannot declare 'owner::work': owner 'owner' is a global variable without a member scope", r"note: owner is declared here"]),
	("record-variable-conflict", "struct Thing {}; i32 Thing;", [r"conflicting declaration kind for 'Thing': declaring global variable after record", r"note: previous declaration is here"]),
	("variable-record-conflict", "i32 Thing; struct Thing {};", [r"conflicting declaration kind for 'Thing': declaring record after global variable", r"note: previous declaration is here"]),
	("alias-variable-conflict", "using Count = i32; i32 Count;", [r"conflicting declaration kind for 'Count': declaring global variable after type alias", r"note: previous declaration is here"]),
	("duplicate-record", "struct Box {}; struct Box {};", [r"duplicate record definition for 'Box' \(record\)", r"note: previous record definition is here"]),
	("duplicate-variable", "i32 value; i64 value;", [r"duplicate declaration of 'value' \(global variable\)", r"note: previous declaration is here"]),
	("duplicate-alias", "using Count = i32; using Count = i64;", [r"duplicate declaration of 'Count' \(type alias\)", r"note: previous declaration is here"]),
	("duplicate-enumerator", "enum Colour { red; red; };", [r"duplicate declaration of 'red' \(enumerator\)", r"note: previous declaration is here"]),
	("destructor-owner", "struct Box { fn ~Other(var Box& this); };", [r"destructor '~Other' shall name its owning class 'Box'"]),
	("destructor-module", "fn ~Other();", [r"destructor '~Other' shall name its owning class; declaration is outside class scope"]),
]


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", default="build/release/bin/rutile-selfhost.exe")
	options = parser.parse_args()
	compiler = ROOT / options.compiler
	output = ROOT / "build" / "compiler-new" / "scope-diagnostics"
	output.mkdir(parents=True, exist_ok=True)

	for name, source, patterns in CASES:
		path = output / (name + ".rt")
		path.write_text(source + "\n", encoding="utf-8")
		result = subprocess.run([str(compiler), str(path), "--output", str(output / (name + ".obj"))], capture_output=True, text=True, timeout=30)
		message = result.stdout + result.stderr
		if result.returncode != 1 or "internal compiler error" in message or any(not re.search(pattern, message) for pattern in patterns):
			raise AssertionError(f"{name}: unexpected diagnostic (exit {result.returncode})\n{message}")
		if "previous declaration" in message:
			assert re.search(re.escape(str(path)) + r":1:\d+: note: previous declaration is here", message), message
		print(name + ": PASS")


if __name__ == "__main__":
	main()
