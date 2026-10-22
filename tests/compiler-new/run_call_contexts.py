"""Verify bounded caller-proof specialization and its native code at optimization zero."""

import argparse
from pathlib import Path
import re
import subprocess

from run import run_case


ROOT = Path(__file__).resolve().parents[2]
TOOLSETS = Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC")
CHECKED = "fn checked(var i32* data, usize index, usize length) -> i32 { if (index >= length) { throw i32(7); }; return data[index]; }; "


def compile_case(compiler, directory, name, source):
	path = directory / (name + ".rt")
	path.write_text(source + "\n", encoding="utf-8")
	output = directory / (name + ".obj")
	result = subprocess.run([str(compiler), str(path), "--output", str(output),
		"--optimization", "0"], capture_output=True, text=True, timeout=60)
	if result.returncode:
		raise RuntimeError(f"{name}: compile failed\n{result.stdout}{result.stderr}")
	tools = sorted(TOOLSETS.glob("*/bin/Hostx64/x64/dumpbin.exe"))
	if not tools:
		raise RuntimeError("dumpbin.exe is required for call-context code verification")
	dumped = subprocess.run([str(tools[-1]), "/nologo", "/disasm", str(output)],
		capture_output=True, text=True, check=True, timeout=30).stdout
	(directory / (name + ".disasm.txt")).write_text(dumped, encoding="utf-8")
	relocations = subprocess.run([str(tools[-1]), "/nologo", "/relocations", str(output)],
		capture_output=True, text=True, check=True, timeout=30).stdout
	(directory / (name + ".relocations.txt")).write_text(relocations, encoding="utf-8")
	functions = {}
	name = None
	for line in dumped.splitlines():
		if line and not line[0].isspace() and line.endswith(":"):
			name = line[:-1]
			if name.startswith(".Lrt.fact."):
				name = name[2:]
			functions[name] = []
		elif name:
			functions[name].append(line)
	return {name: "\n".join(lines) for name, lines in functions.items()}


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", type=Path, default=Path("build/release/native/rutilec.exe"))
	options = parser.parse_args()
	compiler = (ROOT / options.compiler).resolve()
	directory = ROOT / "build/compiler-new/call-contexts"
	directory.mkdir(parents=True, exist_ok=True)
	functions = compile_case(compiler, directory, "shared-context", CHECKED
		+ "fn guarded(var i32* data, usize index, usize length) -> i32 { if (index < length) { return checked(data, index, length); }; return 0; }; "
		+ "fn equivalent(var i32* data, usize index, usize length) -> i32 { if (index >= length) { return 0; }; return checked(data, index, length); }; "
		+ "fn unknown(var i32* data, usize index, usize length) -> i32 { return checked(data, index, length); };")
	contexts = {name: body for name, body in functions.items() if name.startswith("rt.fact.")}
	if len(contexts) != 1:
		raise RuntimeError(f"equivalent proof sites must share one context; found {list(contexts)}")
	if any(re.search(r"\bcmp\s", body) for body in contexts.values()):
		raise RuntimeError("proven checked context retains its bounds comparison at optimization zero")
	if not re.search(r"\bcmp\s", functions.get("checked", "")):
		raise RuntimeError("ordinary checked implementation lost its runtime comparison")
	relocations = (directory / "shared-context.relocations.txt").read_text(encoding="utf-8")
	if len(re.findall(r"\bREL32\b[^\n]*(?:\.L)?rt\.fact\.\d+\s*$", relocations, re.MULTILINE)) != 2:
		raise RuntimeError("both proven call sites must target the shared specialization")
	if len(re.findall(r"\bREL32\b[^\n]*\bchecked\s*$", relocations, re.MULTILINE)) != 1:
		raise RuntimeError("the unknown call site must retain the ordinary implementation")
	print("shared-context-and-unknown-fallback: PASS")
	functions = compile_case(compiler, directory, "wrapper-context", CHECKED
		+ "fn wrapper(var i32* data, usize index, usize length) -> i32 { return checked(data, index, length); }; "
		+ "fn guarded(var i32* data, usize index, usize length) -> i32 { if (index < length) { return wrapper(data, index, length); }; return 0; };")
	contexts = {name: body for name, body in functions.items() if name.startswith("rt.fact.")}
	if len(contexts) != 2 or any(re.search(r"\bcmp\s", body) for body in contexts.values()):
		raise RuntimeError("wrapper and checked contexts must both eliminate proven bounds comparisons")
	relocations = (directory / "wrapper-context.relocations.txt").read_text(encoding="utf-8")
	if len(re.findall(r"\bREL32\b[^\n]*(?:\.L)?rt\.fact\.\d+\s*$", relocations, re.MULTILINE)) != 2:
		raise RuntimeError("guarded wrapper call and specialized delegation must target contexts")
	print("wrapper-call-context: PASS")
	functions = compile_case(compiler, directory, "repeated-access", CHECKED
		+ "fn repeated(var i32* data, usize index, usize length) -> i32 { i32 first = checked(data, index, length); return first + checked(data, index, length); };")
	contexts = {name: body for name, body in functions.items() if name.startswith("rt.fact.")}
	if len(contexts) != 1 or any(re.search(r"\bcmp\s", body) for body in contexts.values()):
		raise RuntimeError("normal return must establish a comparison-free repeated-access context")
	relocations = (directory / "repeated-access.relocations.txt").read_text(encoding="utf-8")
	if len(re.findall(r"\bREL32\b[^\n]*(?:\.L)?rt\.fact\.\d+\s*$", relocations, re.MULTILINE)) != 1:
		raise RuntimeError("second repeated access must target the proven context")
	if len(re.findall(r"\bREL32\b[^\n]*\bchecked\s*$", relocations, re.MULTILINE)) != 1:
		raise RuntimeError("first repeated access must retain its runtime bounds check")
	print("repeated-access-normal-return: PASS")
	functions = compile_case(compiler, directory, "mutated-access", CHECKED
		+ "fn mutated(var i32* data, usize index, usize length, usize replacement) -> i32 { i32 first = checked(data, index, length); @index = replacement; return first + checked(data, index, length); }; "
		+ "fn necessarily_throws(var i32* data) -> i32 { return checked(data, 1, 0); };")
	relocations = (directory / "mutated-access.relocations.txt").read_text(encoding="utf-8")
	if len(re.findall(r"\bREL32\b[^\n]*\bchecked\s*$", relocations, re.MULTILINE)) < 2:
		raise RuntimeError("changing the index must preserve both unknown runtime checks")
	print("mutation-invalidates-and-known-throw-accepted: PASS")
	functions = compile_case(compiler, directory, "indirect-call", CHECKED
		+ "using Callback = ((var i32*, usize, usize) -> i32)*; "
		+ "fn indirect(Callback callback, var i32* data, usize index, usize length) -> i32 { if (index < length) { return callback(data, index, length); }; return 0; };")
	if any(name.startswith("rt.fact.") for name in functions):
		raise RuntimeError("indirect call unexpectedly produced a direct-call specialization")
	print("indirect-call: PASS")
	functions = compile_case(compiler, directory, "virtual-call",
		"struct Base { fn checked(const Base& this, var i32* data, usize index, usize length) -> i32 : [virtual] { if (index >= length) { return 7; }; return data[index]; }; }; "
		+ "fn dispatch(const Base& object, var i32* data, usize index, usize length) -> i32 { if (index < length) { return object.checked(data, index, length); }; return 0; };")
	if any(name.startswith("rt.fact.") for name in functions):
		raise RuntimeError("virtual call unexpectedly produced a direct-call specialization")
	print("virtual-call: PASS")
	functions = compile_case(compiler, directory, "recursive-fallback",
		"fn recursive(usize value) -> usize { if (value < 32) { return recursive(value + 1); }; return value; };")
	if len([name for name in functions if name.startswith("rt.fact.")]) > 32:
		raise RuntimeError("recursive contexts exceeded the bounded specialization budget")
	print("recursive-context-budget: PASS")
	path = directory / "argument-side-effects.rt"
	path.write_text("i32 evaluations; fn observed(var i32* data) -> var i32* { @evaluations = evaluations + 1; return data; }; "
		"fn checked(var i32* data, usize index, usize length) -> i32 { if (index >= length) { return 7; }; return data[index]; }; "
		"fn guarded(var i32* data, usize index, usize length) -> i32 { if (index < length) { return checked(observed(data), index, length); }; return 0; }; "
		"fn main() -> i32 { i32[1] data {}; @data[0] = 41; i32 result = guarded(&@data[0], 0, 1); return result + evaluations; };\n", encoding="utf-8")
	run_case("call-context-argument-side-effects", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	path = directory / "argument-snapshots.rt"
	path.write_text("usize shared = 0; fn change() -> usize { @shared = 1; return 1; }; "
		"fn checked(var i32* data, usize index, usize length) -> i32 { if (index >= length) { return 7; }; return data[index]; }; "
		"fn caller(var i32* data) -> i32 { if (shared < 1) { return checked(data, shared, change()); }; return 1; }; "
		"fn main() -> i32 { i32[2] data {}; @data[0] = 41; @data[1] = 9; i32 answer = caller(&@data[0]); return answer + static_cast<i32>(shared); };\n", encoding="utf-8")
	run_case("call-context-argument-snapshots", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	path = directory / "recursive-runtime.rt"
	path.write_text("fn recursive(usize value) -> usize { if (value < 32) { return recursive(value + 1); }; return value; }; "
		"fn main() -> i32 { return static_cast<i32>(recursive(0)) + 10; };\n", encoding="utf-8")
	run_case("call-context-recursive-runtime", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))


if __name__ == "__main__":
	try:
		main()
	except (OSError, RuntimeError, subprocess.SubprocessError) as error:
		raise SystemExit(str(error))
