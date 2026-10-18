"""Check named, structured compiler failures without linking rejected programs."""

import argparse
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[2]
CASES = [
	("unknown-type", "fn main() -> i32 { Missing value; return 0; };",
		[r"unknown type name 'Missing'"]),
	("value-as-type", "i32 count; fn main() -> i32 { count value; return 0; };",
		[r"name does not denote a type: 'count'", r"note: referenced declaration is here"]),
	("array-extent-type", "fn main() -> i32 { i32[1.5] values; return 0; };",
		[r"array extent shall be a positive integer constant; received type 'f64'"]),
	("array-extent-variable", "fn main() -> i32 { i32 count = 2; i32[count] values; return 0; };",
		[r"array extent shall be a positive integer constant", r"'count' of type 'i32' is not constant"]),
	("array-extent-zero", "fn main() -> i32 { i32[0] values; return 0; };",
		[r"array extent shall be a positive integer constant; received 0"]),
	("array-extent-negative", "fn main() -> i32 { i32[-1] values; return 0; };",
		[r"array extent shall be positive; received -1"]),
	("array-extent-unknown", "fn main() -> i32 { i32[missing] values; return 0; };",
		[r"unknown identifier 'missing'"]),
	("continue-context", "fn main() -> i32 { continue; return 0; };",
		[r"'continue' requires an enclosing loop"]),
	("break-context", "fn main() -> i32 { break; return 0; };",
		[r"'break' requires an enclosing loop or switch"]),
	("discard-place", "fn main() -> i32 { i32 value = 1; discard value; return 0; };",
		[r"discard requires a consumable; operand is a place of type 'i32'"]),
	("discard-unknown", "fn main() -> i32 { discard missing; return 0; };",
		[r"unknown identifier 'missing'"]),
	("allocation-count-type", "fn main() -> i32 { i32* values = new i32[1.5](); return 0; };",
		[r"array allocation count requires an integer; received 'f64'", r"elements of type 'i32'"]),
	("allocation-count-unknown", "fn main() -> i32 { i32* values = new i32[missing](); return 0; };",
		[r"unknown identifier 'missing'"]),
	("delete-type", "fn main() -> i32 { delete 1.5; return 0; };",
		[r"delete requires a pointer; operand has type 'f64'"]),
	("delete-array-type", "fn main() -> i32 { delete[] true; return 0; };",
		[r"delete\[\] requires a pointer; operand has type 'bool'"]),
	("delete-unknown", "fn main() -> i32 { delete missing; return 0; };",
		[r"unknown identifier 'missing'"]),
	("local-reference-initializer", "fn main() -> i32 { i32& value; return 0; };",
		[r"reference 'value' of type 'i32&' requires an initializer"]),
	("local-void-type", "fn main() -> i32 { void value; return 0; };",
		[r"variable 'value' requires object storage or a reference; declared type is 'void'"]),
	("namespace-value", "namespace owner {}; fn main() -> i32 { return owner; };",
		[r"identifier 'owner' does not denote a value", r"note: referenced declaration is here"]),
	("literal-escape", r'fn main() -> i32 { const char* value = "\q"; return 0; };',
		[r"unsupported escape", r"\\\\q", r"in literal"]),
	("context-reference-unknown", "fn take(const i32& value); fn main() -> i32 { take({ @value = missing; }); return 0; };",
		[r"unknown identifier 'missing'"]),
	("context-reference-unbound", "fn take(const i32& value); fn main() -> i32 { take({ }); return 0; };",
		[r"reference contextual argument 'value' of type 'const i32&' requires a bound initializer", r"note: parameter is declared here"]),
	("implicit-constructor-ambiguity", "struct Box { fn Box(Box& this, const i32& value) : [implicit]; fn Box(Box& this, const u32& value) : [implicit]; }; fn make() -> Box { return 42; }; fn main() -> i32 { return 0; };",
		[r"ambiguous implicit conversion from 'i32' to 'Box'", r"multiple conversion functions are applicable",
			r"note: candidate 'Box::Box' has type.*const i32&", r"note: candidate 'Box::Box' has type.*const u32&"]),
	("implicit-function-ambiguity", "struct Box {}; fn operator i32(Box& this) -> i32 : [implicit]; fn operator i32(const Box& this) -> i32 : [implicit]; fn main() -> i32 { Box box; i32 value = @box; return value; };",
		[r"ambiguous implicit conversion from 'Box' to 'i32'", r"note: candidate.*Box&", r"note: candidate.*const Box&"]),
	("constructor-candidates", "struct Box { fn Box(Box& this, i32* pointer); }; fn main() -> i32 { Box value = Box(true); return 0; };",
		[r"no applicable constructor for 'Box' with arguments \(bool\)",
			r"note: candidate 'Box::Box' has type", r"i32\*"]),
	("inaccessible-call", "struct Box { private: fn hidden() -> i32; }; fn main() -> i32 { return Box::hidden(); };",
		[r"selected function 'Box::hidden' of type '\(\) -> i32' is not accessible from this scope",
			r"note: function is declared here"]),
	("indirect-call-count", "fn take(i32 value) -> i32; fn main() -> i32 { ((i32) -> i32)* callback = &take; return callback(); };",
		[r"call to function of type '\(i32\) -> i32' requires 1 arguments, received 0 with types \(\)"]),
	("template-deduction-conflict", "namespace math { template<typename T> fn choose(T first, T second) -> T; }; fn main() -> i32 { return math::choose(i32(42), true); };",
		[r"cannot deduce template arguments for 'math::choose'", r"arguments \(i32, bool\)",
			r"call argument 2 of type 'bool' does not match parameter 2 'second'",
			r"note: function template is declared here", r"note: deduction failed for this parameter type"]),
	("template-deduction-shape", "template<typename T> fn load(T* pointer) -> T; fn main() -> i32 { return load(i32(42)); };",
		[r"cannot deduce template arguments for 'load'", r"arguments \(i32\)",
			r"call argument 1 of type 'i32' does not match parameter 1 'pointer'",
			r"note: function template is declared here"]),
	("literal-local", "fn main() -> i32 { var answer := 42; return answer; };",
		[r"cannot infer the type of variable 'answer'", r"untyped numeric literal"]),
	("literal-global", "var answer := 42; fn main() -> i32 { return answer; };",
		[r"cannot infer the type of variable 'answer'", r"specify a variable type"]),
	("literal-field", "struct Box { var answer := 42; }; fn main() -> i32 { return 0; };",
		[r"cannot infer the type of variable 'answer'", r"untyped numeric literal"]),
	("array-construction", "fn main() -> i32 { char* bytes = new char[42]; return 0; };",
		[r"array allocation requires a construction argument list", r"new T\[count\]\(\)"]),
	("conversion-types", "fn main() -> i32 { i32* pointer = true; return 0; };",
		[r"cannot initialize 'i32\*'", r"consumable of type 'bool'"]),
	("call-candidates", "fn take(i32* value); fn main() -> i32 { take(true); return 0; };",
		[r"no applicable function for call to 'take'", r"arguments \(bool\)",
			r"note: candidate 'take' has type '\(i32\*\) -> void'"]),
	("template-count", "template<typename T> struct Box {}; Box<i32, i32> value;",
		[r"too many template arguments for 'Box'", r"expected at most 1, received 2",
			r"note: template is declared here"]),
	("template-missing", "template<typename T> struct Box {}; Box<> value;",
		[r"missing argument for template parameter 'T' of 'Box'", r"no default",
			r"note: template parameter is declared here"]),
	("template-call-count", "template<typename T> fn take(T value); fn main() -> i32 { take<i32>(); return 0; };",
		[r"function template 'take' expects 1 call arguments, received 0", r"note: template is declared here"]),
	("template-value", "i32 count; template<i32 N> struct Box {}; Box<count> value;",
		[r"value parameter 'N'", r"integer constant of type 'i32'", r"note: value parameter"]),
	("unknown-qualified", "namespace owner {}; fn main() -> i32 { return owner::missing; };",
		[r"unknown identifier 'owner::missing'"]),
	("duplicate-local", "fn main() -> i32 { i32 answer; i32 answer; return 0; };",
		[r"duplicate local declaration of 'answer'", r"note: previous declaration is here"]),
	("deleted-function", "fn forbidden() : [delete]; fn main() -> i32 { forbidden(); return 0; };",
		[r"selected function 'forbidden'", r"is deleted", r"note: function is declared deleted here"]),
]


def main():
	arguments = argparse.ArgumentParser(description=__doc__)
	arguments.add_argument("--compiler", default="build/release/bin/rutile-selfhost.exe")
	options = arguments.parse_args()
	directory = ROOT / "build" / "compiler-new" / "diagnostics"
	directory.mkdir(parents=True, exist_ok=True)
	compiler = ROOT / options.compiler
	for name, source, patterns in CASES:
		path = directory / (name + ".rt")
		path.write_text(source + "\n", encoding="utf-8")
		result = subprocess.run([str(compiler), str(path), "--output",
			str(directory / (name + ".obj"))], capture_output=True, text=True, timeout=30)
		output = result.stdout + result.stderr
		if result.returncode != 1 or "internal compiler error" in output or len(re.findall(r": error:", output)) != 1 or any(not re.search(pattern, output) for pattern in patterns):
			raise RuntimeError(f"{name}: unexpected diagnostic (exit {result.returncode})\n{output}")
		print(name + ": PASS")

	path = directory / "typed-inference.rt"
	path.write_text("""fn main() -> i32 {
	var answer = i32(42);
	var character = 'x';
	var condition = 1 < 2;
	var computed = answer + 1;
	char* bytes = new char[42]();
	delete[] bytes;
	return answer;
};
""", encoding="utf-8")
	result = subprocess.run([str(compiler), str(path), "--output",
		str(directory / "typed-inference.obj")], capture_output=True, text=True, timeout=30)
	if result.returncode:
		raise RuntimeError("typed-inference failed\n" + result.stdout + result.stderr)
	print("typed-inference: PASS")


if __name__ == "__main__":
	main()
