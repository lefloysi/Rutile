"""Verify template diagnostics name actual declarations, inputs and substitution context."""

import argparse
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[2]
CASES = [
	("qualified-owner", "template<typename T> fn Missing<T>::read();",
		[r"template declaration 'Missing<T>::read'", r"declared owner template"], []),
	("template-conflict", "i32 Box; template<typename T> struct Box {};",
		[r"template declaration 'Box'", r"conflicts with another declaration", r"note: conflicting declaration is here"], []),
	("template-redeclaration", "template<typename T> struct Box; template<typename T, typename U> struct Box;",
		[r"template redeclaration 'Box'", r"previous declaration has 1 parameters, received 2", r"note: previous template declaration is here"], []),
	("template-duplicate", "template<typename T> struct Box {}; template<typename T> struct Box {};",
		[r"template declaration 'Box'", r"duplicate template definition", r"note: previous template definition is here"], []),
	("automatic-duplicate", "fn take(auto i32& value) {}; fn take(auto i32& value) {};",
		[r"function template 'take'", r"duplicate function definition", r"note: previous declaration is here"], []),
	("non-template-application", "struct Plain {}; Plain<i32> value;",
		[r"template arguments for 'Plain' require a template declaration", r"resolved declaration is 'Plain'", r"note: non-template declaration is here"], []),
	("unknown-type-argument", "template<typename T> struct Box {}; Box<Missing> value;",
		[r"Missing", r"note: while checking argument 1 for parameter 'T' of template 'Box'"], []),
	("value-argument-role", "template<i32 Count> struct Box {}; Box<var i32*> value;",
		[r"value parameter 'Count' of template 'Box' at position 1", r"requires a value expression", r"note: value parameter is declared here"], []),
	("nonconstant-value", "i32 runtime; template<i32 Count> struct Box {}; Box<runtime> value;",
		[r"value parameter 'Count' of template 'Box' at position 1", r"integer constant of type 'i32'", r"supplied expression 'runtime' has type 'i32'"], []),
	("default-type-context", "template<typename T = Missing> struct Box {}; Box<> value;",
		[r"Missing", r"note: while evaluating default argument for parameter 'T' of template 'Box'", r"note: template instantiation is requested here"], []),
	("default-value-context", "i32 runtime; template<i32 Count = runtime> struct Box {}; Box<> value;",
		[r"integer constant of type 'i32'", r"note: while evaluating default argument for parameter 'Count' of template 'Box'", r"note: template instantiation is requested here"], []),
	("call-arity-types", "template<typename T> fn take(T value); fn main()->i32 { take<i32>(i32(1), true); return 0; };",
		[r"function template 'take' expects 1 call arguments, received 2 with types \(i32, bool\)", r"note: template is declared here"], []),
	("deduction-conflict", "template<typename T> fn same(T first, T second)->T; fn main()->i32 { return same(i32(1), true); };",
		[r"cannot deduce template arguments for 'same' with arguments \(i32, bool\)", r"call argument 2 of type 'bool' does not match parameter 2 'second'", r"note: deduction failed for this parameter type", r"note: template parameter 'T' is bound to type 'i32'"], []),
	("explicit-and-deduced-bindings", "template<typename T, typename U> fn mix(T fixed, U first, U second)->i32; fn main()->i32 { return mix<i64>(i64(1), i32(2), true); };",
		[r"cannot deduce template arguments for 'mix'", r"note: template parameter 'T' is bound to type 'i64'", r"note: template parameter 'U' is bound to type 'i32'"], []),
	("signed-value-binding", "template<i32 N> struct Number {}; template<i32 N, typename T> fn clash(Number<N> number, T first, T second)->i32; fn main()->i32 { return clash(Number<-2>(), i32(1), true); };",
		[r"cannot deduce template arguments for 'clash'", r"note: template parameter 'N' is bound to value -2 of type 'i32'", r"note: template parameter 'T' is bound to type 'i32'"], []),
	("record-instantiation-context", "template<typename T> struct Box { T::Missing value; }; Box<i32> value;",
		[r"Missing", r"note: while instantiating 'Box<i32>' requested here"], []),
	("deferred-function-context", "struct Item {}; template<typename T> fn read(T value)->i32 { return value.missing; }; fn main()->i32 { return read(Item()); };",
		[r"missing", r"note: .*read<Item>.*requested here"], []),
	("nested-instantiation-context", "template<typename T> struct Inner { T::Missing value; }; template<typename T> struct Outer { Inner<T> value; }; Outer<i32> value;",
		[r"Missing", r"note: while instantiating 'Inner<i32>' requested here", r"note: while instantiating 'Outer<i32>' requested here"], []),
	("automatic-shape", "fn load(auto i32* value)->i32; fn main()->i32 { return load(i32(42)); };",
		[r"cannot deduce automatic qualification for function template 'load' with argument types \(i32\)", r"note: function template is declared here"], []),
	("hidden-automatic-argument", "fn take(auto i32& value) : [delete]; fn main()->i32 { take(i32(42)); return 0; };",
		[r"selected function 'take'", r"is deleted"], [r"take<"]),
	("hidden-explicit-readonly", "template<typename T> fn take(auto T& value) : [delete]; fn main()->i32 { take<i32>(i32(42)); return 0; };",
		[r"selected function 'take<i32>'", r"is deleted"], [r"take<i32,"]),
	("hidden-explicit-mutable", "template<typename T> fn take(auto T& value) : [delete]; fn main()->i32 { i32 value=42; take<i32>(@value); return 0; };",
		[r"selected function 'take<i32>'", r"is deleted"], [r"take<i32,"]),
]


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", default=".kiln/build/release/target-727574696c65632e657865/rutilec.exe")
	options = parser.parse_args()
	compiler = ROOT / options.compiler
	output = ROOT / "build" / "compiler-new" / "template-diagnostics"
	output.mkdir(parents=True, exist_ok=True)

	for name, source, required, forbidden in CASES:
		path = output / (name + ".rt")
		path.write_text(source + "\n", encoding="utf-8")
		result = subprocess.run([str(compiler), str(path), "--output", str(output / (name + ".obj"))], capture_output=True, text=True, timeout=30)
		message = result.stdout + result.stderr
		if result.returncode != 1 or "internal compiler error" in message or any(not re.search(pattern, message) for pattern in required) or any(re.search(pattern, message) for pattern in forbidden):
			raise AssertionError(f"{name}: unexpected diagnostic (exit {result.returncode})\n{message}")
		if name == "nested-instantiation-context":
			inner = source.index("Inner<T>") + len("Inner<") + 1
			outer = source.index("Outer<i32>") + len("Outer<") + 1
			inner_note = f":1:{inner}: note: while instantiating 'Inner<i32>' requested here"
			outer_note = f":1:{outer}: note: while instantiating 'Outer<i32>' requested here"
			assert inner_note in message and outer_note in message, message
			assert message.index(inner_note) < message.index(outer_note), message
		if name == "deferred-function-context":
			call = source.rindex("read(") + len("read") + 1
			assert f":1:{call}: note: while instantiating function 'read<Item>' requested here" in message, message
		print(name + ": PASS")


if __name__ == "__main__":
	main()
