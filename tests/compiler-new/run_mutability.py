"""Verify mutable values and readonly targets of unqualified pointers and references."""

import argparse
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
RECORD = """struct Foo {
	i32 value;
};

"""
CASES = [
	("generic-intrinsic-const-reference", RECORD + """template<typename T> fn change(var T& value) {
	@value.value = 42;
};

fn main() -> i32 {
	const Foo value;
	change<const Foo>(value);
	return 0;
};
""", False),
	("generic-intrinsic-const-pointer", RECORD + """template<typename T> fn change(var T* value) {
	@value->value = 42;
};

fn main() -> i32 {
	const Foo value;
	change<const Foo>(&value);
	return 0;
};
""", False),
	("alias-intrinsic-const-reference", RECORD + """using ReadonlyFoo = const Foo;

fn main() -> i32 {
	const Foo value;
	var ReadonlyFoo& borrowed = value;
	@borrowed.value = 42;
	return 0;
};
""", False),
	("alias-intrinsic-const-pointer", RECORD + """using ReadonlyFoo = const Foo;

fn main() -> i32 {
	const Foo value;
	var ReadonlyFoo* borrowed = &value;
	@borrowed->value = 42;
	return 0;
};
""", False),
	("inferred-address-readonly", RECORD + """fn main() -> i32 {
	Foo value;
	var pointer = &value;
	@pointer->value = 42;
	return 0;
};
""", False),
	("inferred-address-mutable", RECORD + """fn main() -> i32 {
	Foo value;
	var pointer = &@value;
	@pointer->value = 42;
	return value.value;
};
""", True),
	("inferred-allocation-readonly", RECORD + """fn main() -> i32 {
	var pointer = new Foo();
	@pointer->value = 42;
	return 0;
};
""", False),
	("inferred-allocation-mutable", RECORD + """fn main() -> i32 {
	var pointer = new var Foo();
	@pointer->value = 42;
	i32 result = pointer->value;
	delete pointer;
	return result;
};
""", True),
	("value-default", RECORD + "fn main() -> i32 {\n\tFoo value;\n\t@value.value = 42;\n\treturn value.value;\n};\n", True),
	("scalar-default", "fn main() -> i32 {\n\ti32 value = 1;\n\t@value = 42;\n\treturn value;\n};\n", True),
	("value-const", RECORD + "fn main() -> i32 {\n\tconst Foo value;\n\t@value.value = 42;\n\treturn 0;\n};\n", False),
	("member-default", RECORD + "fn main() -> i32 {\n\tFoo value;\n\t@value.value = 42;\n\treturn value.value;\n};\n", True),
	("member-const", "struct Foo {\n\tconst i32 value;\n};\n\nfn main() -> i32 {\n\tFoo value;\n\t@value.value = 42;\n\treturn 0;\n};\n", False),
	("parameter-default", "fn change(i32 value) -> i32 {\n\t@value = 42;\n\treturn value;\n};\n\nfn main() -> i32 {\n\treturn change(1);\n};\n", True),
	("parameter-const", "fn change(const i32 value) -> i32 {\n\t@value = 42;\n\treturn value;\n};\n\nfn main() -> i32 {\n\treturn change(1);\n};\n", False),
	("reference-readonly", RECORD + "fn main() -> i32 {\n\tFoo value;\n\tFoo& borrowed = value;\n\t@borrowed.value = 42;\n\treturn 0;\n};\n", False),
	("reference-mutable", RECORD + "fn main() -> i32 {\n\tFoo value;\n\tvar Foo& borrowed = @value;\n\t@borrowed.value = 42;\n\treturn value.value;\n};\n", True),
	("pointer-reassign-default", RECORD + "fn main() -> i32 {\n\tFoo first;\n\tFoo second;\n\t@second.value = 42;\n\tFoo* pointer = &first;\n\t@pointer = &second;\n\treturn pointer->value;\n};\n", True),
	("pointer-target-readonly", RECORD + "fn main() -> i32 {\n\tFoo value;\n\tFoo* pointer = &value;\n\t@pointer->value = 42;\n\treturn 0;\n};\n", False),
	("pointer-target-mutable", RECORD + "fn main() -> i32 {\n\tFoo value;\n\tvar Foo* pointer = &@value;\n\t@pointer->value = 42;\n\treturn value.value;\n};\n", True),
	("pointer-object-const", RECORD + "fn main() -> i32 {\n\tFoo first;\n\tFoo second;\n\tconst (Foo*) pointer = &first;\n\t@pointer = &second;\n\treturn 0;\n};\n", False),
	("pointer-removes-readonly", RECORD + "fn main() -> i32 {\n\tFoo value;\n\tFoo* borrowed = &value;\n\tvar Foo* pointer = borrowed;\n\t@pointer->value = 42;\n\treturn 0;\n};\n", False),
	("nested-intermediate-readonly", RECORD + "fn main() -> i32 {\n\tFoo first;\n\tFoo second;\n\tFoo* inner = &first;\n\tFoo** outer = &inner;\n\t@*outer = &second;\n\treturn 0;\n};\n", False),
	("nested-intermediate-mutable", RECORD + "fn main() -> i32 {\n\tFoo first;\n\tFoo second;\n\t@second.value = 42;\n\tFoo* inner = &first;\n\tFoo var** outer = &@inner;\n\t@*outer = &second;\n\treturn (*outer)->value;\n};\n", True),
	("nested-final-target-readonly", RECORD + "fn main() -> i32 {\n\tFoo value;\n\tFoo* inner = &value;\n\tFoo var** outer = &@inner;\n\t@(*outer)->value = 42;\n\treturn 0;\n};\n", False),
	("nested-both-targets-mutable", RECORD + "fn main() -> i32 {\n\tFoo value;\n\tvar Foo* inner = &@value;\n\tvar (var Foo*)* outer = &@inner;\n\t@(*outer)->value = 42;\n\treturn value.value;\n};\n", True),
	("nested-cannot-remove-readonly", RECORD + "fn main() -> i32 {\n\tFoo value;\n\tFoo* inner = &value;\n\tFoo** borrowed = &inner;\n\tvar (var Foo*)* outer = borrowed;\n\treturn 0;\n};\n", False),
	("array-target-readonly", "fn main() -> i32 {\n\ti32[2] values {};\n\ti32[2]* pointer = &values;\n\t@(*pointer)[0] = 42;\n\treturn 0;\n};\n", False),
	("array-target-mutable", "fn main() -> i32 {\n\ti32[2] values {};\n\tvar (i32[2])* pointer = &@values;\n\t@(*pointer)[0] = 42;\n\treturn values[0];\n};\n", True),
	("inferred-var", RECORD + "fn main() -> i32 {\n\tvar value = Foo();\n\t@value.value = 42;\n\treturn value.value;\n};\n", True),
	("name-first", "fn main() -> i32 {\n\tvar value: i32 = 1;\n\t@value = 42;\n\treturn value;\n};\n", True),
	("name-first-pointer", RECORD + "fn main() -> i32 {\n\tFoo first;\n\tFoo second;\n\t@second.value = 42;\n\tvar pointer: Foo* = &first;\n\t@pointer = &second;\n\treturn pointer->value;\n};\n", True),
	("template-owned-value", RECORD + "template<typename T> fn change(T value) -> i32 {\n\t@value.value = 42;\n\treturn value.value;\n};\n\nfn main() -> i32 {\n\treturn change<Foo>(Foo());\n};\n", True),
	("template-reference-readonly", RECORD + "template<typename T> fn change(T& value) {\n\t@value.value = 42;\n};\n\nfn main() -> i32 {\n\tFoo value;\n\tchange<Foo>(@value);\n\treturn 0;\n};\n", False),
	("template-reference-mutable", RECORD + "template<typename T> fn change(var T& value) {\n\t@value.value = 42;\n};\n\nfn main() -> i32 {\n\tFoo value;\n\tchange<Foo>(@value);\n\treturn value.value;\n};\n", True),
	("conditional-auto", RECORD + "fn Foo::data(auto Foo& this) -> const[this] i32& {\n\treturn @this.value;\n};\n\nfn main() -> i32 {\n\tFoo value;\n\t@(@value).data() = 42;\n\treturn value.data();\n};\n", True),
	("conditional-auto-readonly", RECORD + "fn Foo::data(auto Foo& this) -> const[this] i32& {\n\treturn @this.value;\n};\n\nfn main() -> i32 {\n\tconst Foo value;\n\t@value.data() = 42;\n\treturn 0;\n};\n", False),
	("const-cast", "fn main() -> i32 {\n\ti32 value = 1;\n\ti32& borrowed = value;\n\t@const_cast<var i32&>(borrowed) = 42;\n\treturn value;\n};\n", True),
	("buffer-values", "fn main() -> i32 {\n\trt::buffer<i32> values;\n\tif (!@values.push_back(1)) {\n\t\treturn 1;\n\t};\n\tvar i32* element = (@values).data();\n\t@*element = 42;\n\treturn *element;\n};\n", True),
	("buffer-pointer-values", "fn main() -> i32 {\n\ti32 first = 1;\n\ti32 second = 42;\n\trt::buffer<i32*> values;\n\tif (!@values.push_back(&first)) {\n\t\treturn 1;\n\t};\n\t@*(@values).data() = &second;\n\treturn **(@values).data();\n};\n", True),
	("buffer-pointer-target-readonly", "fn main() -> i32 {\n\ti32 value = 1;\n\trt::buffer<i32*> values;\n\t@values.push_back(&value);\n\t@**(@values).data() = 42;\n\treturn 0;\n};\n", False),
]


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", default="build/release/bin/rutile-selfhost.exe")
	parser.add_argument("--build", default="build/release", type=Path)
	parser.add_argument("--standard-directory", default="kiln/compiler/std", type=Path)
	options = parser.parse_args()
	build = (ROOT / options.build).resolve()
	cache = (build / "CMakeCache.txt").read_text()
	runtime = "/MDd" if "CMAKE_BUILD_TYPE:STRING=Debug" in cache else "/MD"
	directory = build / "tests/mutability"
	directory.mkdir(parents=True, exist_ok=True)
	toolchain = Path("C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/Tools/VsDevCmd.bat")
	anchor = directory / "anchor.cpp"
	anchor.write_text("int mutability_test_anchor = 0;\n")
	failures = []
	for name, source, accepted in CASES:
		path = directory / (name + ".rt")
		output = directory / (name + ".obj")
		path.write_text(source, encoding="utf-8")
		dependencies = []
		if name.startswith("buffer-"):
			dependencies = [str((ROOT / options.standard_directory / (item + ".rt")).resolve()) for item in ["unique", "buffer"]]
		compiled = subprocess.run([str((ROOT / options.compiler).resolve()), *dependencies, str(path),
			"--output", str(output)], cwd=directory, capture_output=True, text=True, timeout=60)
		try:
			message = compiled.stdout + compiled.stderr
			if not accepted:
				if compiled.returncode != 1 or "error:" not in message or "internal compiler error" in message:
					raise RuntimeError(f"expected ordinary rejection, exit {compiled.returncode}\n{message}")
				if "error: expected" in message:
					raise RuntimeError("mutation fixture failed parsing instead of qualification checking\n" + message)
				if "intrinsic-const" in name:
					mutation_line = next(index for index, line in enumerate(source.splitlines(), 1) if "@" in line)
					if not re.search(rf":{mutation_line}:\d+: error:", message):
						raise RuntimeError("intrinsic const fixture failed before its mutation\n" + message)
			elif compiled.returncode:
				raise RuntimeError(f"expected acceptance, exit {compiled.returncode}\n{message}")
			else:
				executable = directory / (name + ".exe")
				command = ["cl", "/nologo", "/EHsc", runtime, str(anchor), str(output),
					"/Fe:" + str(executable), "/link", "legacy_stdio_definitions.lib"]
				linked = subprocess.run(f'cmd /d /s /c ""{toolchain}" -no_logo -arch=x64 && {subprocess.list2cmdline(command)}"',
					cwd=directory, capture_output=True, text=True, timeout=60)
				if linked.returncode:
					raise RuntimeError("link failed\n" + linked.stdout + linked.stderr)
				result = subprocess.run([str(executable)], cwd=directory, capture_output=True, text=True, timeout=30)
				if result.returncode != 42:
					raise RuntimeError(f"expected runtime result 42, got {result.returncode}\n{result.stdout}{result.stderr}")
			print(name + ": PASS", flush=True)
		except RuntimeError as error:
			failures.append(name)
			print(name + ": " + str(error), file=sys.stderr, flush=True)
	print(f"Mutability: {len(CASES) - len(failures)} passed, {len(failures)} failed")
	return bool(failures)


if __name__ == "__main__":
	try:
		sys.exit(main())
	except (OSError, subprocess.TimeoutExpired) as error:
		print(error, file=sys.stderr)
		sys.exit(1)
