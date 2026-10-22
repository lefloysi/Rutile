"""Check shared frontend diagnostics preserve expected forms and expose actual source tokens."""

import argparse
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[2]
CASES = [
	("unknown-character", "fn main()->i32 {return #;};", "#", [r"unknown source character; found '#'" ]),
	("source-nul", "fn main()->i32 {return \u0000;};", "\0", [r"NUL byte in source; found '\\0'"]),
	("exponent-digits", "fn main()->i32 {return 1e+;};", "1e+", [r"expected exponent digits; found '1e\+'"]),
	("unterminated-string", "\"unfinished", '"', [r"unterminated string literal; found '\"unfinished'", r"^(?!.*found end of file)" ]),
	("string-newline", "\"first\nsecond", '"', [r"newline in string literal; found '\"first'"]),
	("unterminated-comment", "/* unfinished", "/*", [r"unterminated comment; found '/\* unfinished'"]),
	("actual-delimiter", "fn take(i32 value];", "]", [r"expected.*; found '\]'" ]),
	("actual-closing-brace", "fn main()->i32 {return 1}", "}", [r"expected.*; found '}'"]),
	("parser-eof", "fn main()->i32 {return 1;", None, [r"expected.*; found end of file"]),
]


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", default=".kiln/build/release/target-727574696c65632e657865/rutilec.exe")
	options = parser.parse_args()
	compiler = ROOT / options.compiler
	output = ROOT / "build" / "compiler-new" / "frontend-diagnostics"
	output.mkdir(parents=True, exist_ok=True)

	for name, source, marker, patterns in CASES:
		path = output / (name + ".rt")
		path.write_bytes(source.encode("utf-8"))
		result = subprocess.run([str(compiler), str(path), "--output", str(output / (name + ".obj"))], capture_output=True, text=True, timeout=30)
		message = result.stdout + result.stderr
		offset = source.index(marker) if marker is not None else len(source)
		prefix = source[:offset]
		line = prefix.count("\n") + 1
		column = len(prefix.rsplit("\n", 1)[-1]) + 1
		location = re.escape(str(path)) + f":{line}:{column}: error:"
		if result.returncode != 1 or "internal compiler error" in message or not re.search(location, message, re.IGNORECASE) or any(not re.search(pattern, message) for pattern in patterns):
			raise AssertionError(f"{name}: unexpected diagnostic (exit {result.returncode})\n{message}")
		print(name + ": PASS")


if __name__ == "__main__":
	main()
