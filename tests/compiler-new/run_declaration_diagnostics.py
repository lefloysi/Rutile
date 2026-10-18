"""Check contextual declaration diagnostics and their actual declaration notes."""

import argparse
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[2]
CASES = [
	("duplicate-parameter", "fn take(i32 repeated, i64 repeated);",
		[r"duplicate parameter 'repeated' in function 'take'", r"note: previous parameter is declared here"]),
	("invalid-parameter", "fn take(void item);",
		[r"parameter 1 'item' of function 'take'", r"object or reference type", r"received 'void'"]),
	("unknown-attribute", "fn take() : [mystery];",
		[r"declaration 'take'", r"unknown function attribute 'mystery'"]),
	("conversion-result", "fn operator i32(const bool& value) -> bool;",
		[r"conversion function result does not match its named target", r"result is 'bool'", r"target is 'i32'"]),
	("constructor-receiver", "struct Box { fn Box(i32 wrong); };",
		[r"declaration 'Box::Box'", r"expected first parameter 'Box& this'", r"received 'i32 wrong'"]),
	("constructor-result", "struct Box { fn Box(var Box& this) -> i32; };",
		[r"declaration 'Box::Box'", r"no result annotation", r"declared signature", r"i32"]),
	("enum-underlying", "enum Colour : [f64] {};",
		[r"declaration 'Colour'", r"integer type", r"received 'f64'"]),
	("duplicate-definition", "fn take() -> i32 { return 1; }; fn take() -> i32 { return 2; };",
		[r"duplicate function definition for 'take'", r"of type '\(\) -> i32'", r"note: previous definition is here"]),
	("template-duplicate-parameter", "template<typename T, typename T> struct Box {};",
		[r"duplicate template parameter 'T' at position 2", r"note: previous template parameter is declared here"]),
	("template-parameter-type", "template<f64 Count> struct Box {};",
		[r"non-type template parameter 'Count' at position 1", r"requires an integral type; received 'f64'"]),
	("template-duplicate-member", "template<typename T> struct Box { i32 repeated; i32 repeated; };",
		[r"duplicate declaration of 'repeated' in template member scope", r"note: previous declaration is here"]),
	("template-constructor-receiver", "template<typename T> struct Box { fn Box(); };",
		[r"instance function 'Box::Box'", r"explicit first reference parameter named 'this'"]),
]


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", default="build/release/bin/rutile-selfhost.exe")
	options = parser.parse_args()
	compiler = ROOT / options.compiler
	output = ROOT / "build" / "compiler-new" / "declaration-diagnostics"
	output.mkdir(parents=True, exist_ok=True)

	for name, source, patterns in CASES:
		path = output / (name + ".rt")
		path.write_text(source + "\n", encoding="utf-8")
		result = subprocess.run([str(compiler), str(path), "--output", str(output / (name + ".obj"))], capture_output=True, text=True, timeout=30)
		message = result.stdout + result.stderr
		if result.returncode != 1 or "internal compiler error" in message or any(not re.search(pattern, message) for pattern in patterns):
			raise AssertionError(f"{name}: unexpected diagnostic (exit {result.returncode})\n{message}")
		print(name + ": PASS")


if __name__ == "__main__":
	main()
