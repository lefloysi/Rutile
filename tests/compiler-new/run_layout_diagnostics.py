"""Check native layout and external ABI errors retain actual source declarations and types."""

import argparse
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[2]
COUNT = "2305843009213693952"
OVERFLOW = [r"array storage size exceeds target address space for type", COUNT, r"elements of 8 bytes", r"64-bit target limit of 18446744073709551615 bytes"]
CASES = [
	("local-overflow", "fn main()->i32 { i64[2305843009213693952] huge; return 0; };", "huge", OVERFLOW),
	("global-overflow", "i64[2305843009213693952] huge; fn main()->i32 { return 0; };", "huge", OVERFLOW),
	("field-overflow", "struct Box { i64[2305843009213693952] huge; }; fn main()->i32 { Box value; return 0; };", "huge", OVERFLOW),
	("nested-overflow", "fn main()->i32 { i64[2][1152921504606846976] huge; return 0; };", "huge", [r"array storage size exceeds target address space", r"64-bit target limit"]),
	("parameter-overflow", "fn take(i64[2305843009213693952] huge) { }; fn main()->i32 { return 0; };", "huge", OVERFLOW),
	("sizeof-overflow", "fn main()->i32 { sizeof(i64[2305843009213693952]); return 0; };", "sizeof", OVERFLOW),
	("pointer-dereference-overflow", "fn main()->i32 { var (i64[2305843009213693952])* pointer=nullptr; *pointer; return 0; };", "*pointer", OVERFLOW),
	("pointer-index-address-overflow", "fn main()->i32 { var (i64[2305843009213693952])* pointer=nullptr; &@pointer[0]; return 0; };", "[0]", OVERFLOW),
	("indirect-function-result-overflow", "fn main()->i32 { (() -> i64[2305843009213693952])* callback=nullptr; callback(); return 0; };", "(); return", OVERFLOW),
	("pointer-offset-overflow", "fn main()->i32 { var (i64[2305843009213693952])* pointer=nullptr; pointer+1; return 0; };", "+1", OVERFLOW),
	("pointer-member-address-overflow", "struct Box { i64[2305843009213693952] field; }; fn main()->i32 { var Box* pointer=nullptr; &@pointer->field; return 0; };", "field;", OVERFLOW),
	("pointer-inplace-constructor-overflow", "struct Box { i64[2305843009213693952] field; }; fn main()->i32 { var Box* pointer=nullptr; @pointer->Box(); return 0; };", "field;", OVERFLOW),
	("external-record-result", "struct Box { i32 value; }; fn external()->Box; fn main()->i32 { return 0; };", "external", [r"external aggregate return requires unsupported C ABI lowering for function 'external'", r"of type '\(\) -> Box'", r"result type is 'Box'"]),
	("external-record-parameter", "struct Box { i32 value; }; fn external(i32 first, Box object); fn main()->i32 { return 0; };", "object", [r"external aggregate parameter requires unsupported C ABI lowering for parameter 2 of function 'external'", r"parameter type is 'Box'"]),
	("external-array-parameter", "fn external(i32 first, i32[2] values); fn main()->i32 { return 0; };", "values", [r"external aggregate parameter requires unsupported C ABI lowering for parameter 2 of function 'external'", r"parameter type is 'i32\[2\]'"]),
]


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", default="build/release/bin/rutile-selfhost.exe")
	options = parser.parse_args()
	compiler = ROOT / options.compiler
	output = ROOT / "build" / "compiler-new" / "layout-diagnostics"
	output.mkdir(parents=True, exist_ok=True)

	for name, source, marker, patterns in CASES:
		path = output / (name + ".rt")
		path.write_text(source + "\n", encoding="utf-8")
		result = subprocess.run([str(compiler), str(path), "--output", str(output / (name + ".obj"))], capture_output=True, text=True, timeout=30)
		message = result.stdout + result.stderr
		column = source.index(marker) + 1
		location = re.escape(str(path)) + f":1:{column}: error:"
		if result.returncode != 1 or "internal compiler error" in message or not re.search(location, message, re.IGNORECASE) or any(not re.search(pattern, message) for pattern in patterns):
			raise AssertionError(f"{name}: unexpected diagnostic (exit {result.returncode})\n{message}")
		assert len(re.findall(r": error:", message)) == 1, message
		print(name + ": PASS")

	path = output / "supported-layouts.rt"
	path.write_text("struct Box { i32 value; }; fn external(const Box& object); fn main()->i32 { i64[2] values; return 0; };\n", encoding="utf-8")
	result = subprocess.run([str(compiler), str(path), "--output", str(output / "supported-layouts.obj")], capture_output=True, text=True, timeout=30)
	if result.returncode:
		raise AssertionError("supported layouts rejected\n" + result.stdout + result.stderr)
	print("supported-layouts: PASS")


if __name__ == "__main__":
	main()
