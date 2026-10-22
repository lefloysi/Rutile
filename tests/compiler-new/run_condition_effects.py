"""Verify proven conditions retain operand effects, ordering, and exception behavior."""

import argparse
from pathlib import Path
import re
import subprocess

from run_call_contexts import compile_case


ROOT = Path(__file__).resolve().parents[2]
OBSERVE = "i32 calls; fn observed() -> i32 { @calls = calls + 1; return 1; }; "
CODEGEN = [
	("comparison", OBSERVE + "fn inspect() -> i32 { if (observed() == 1) { return 42; }; return 0; };", "inspect", 0, "observed", 1),
	("both-operands", OBSERVE + "fn inspect() -> i32 { if (observed() == observed()) { return 42; }; return 0; };", "inspect", 0, "observed", 2),
	("boolean-call", "i32 calls; fn observed() -> bool { @calls = calls + 1; return true; }; fn inspect() -> i32 { if (observed()) { return 42; }; return 0; };", "inspect", 0, "observed", 1),
	("negated-call", "i32 calls; fn observed() -> bool { @calls = calls + 1; return false; }; fn inspect() -> i32 { if (!observed()) { return 42; }; return 0; };", "inspect", 0, "observed", 1),
	("unknown", "fn observed() -> i32; fn inspect(i32 limit) -> i32 { if (observed() < limit) { return 42; }; return 0; };", "inspect", 1, "observed", 1),
]
RUNTIME = [
	("assignment-rhs-first", "fn main() -> i32 { i32[2] data {}; usize index = 2; @data[index] = i32(@index = 0) + 42; return data[0]; };"),
	("index-offset-first", "fn main() -> i32 { i32[2][2] data {}; @data[0][0] = 1; @data[0][1] = 42; usize index = 1; return data[(@index = 0)][index]; };"),
	("reconstruction-destination-first", "fn main() -> i32 { i32[2] data {}; usize index = 1; @data[index].i32((@index = 0) + 42); return data[1]; };"),
	("operand-order", "i32 trace; fn first() -> i32 { @trace = trace * 10 + 1; return 7; }; fn second() -> i32 { @trace = trace * 10 + 2; return 7; }; fn main() -> i32 { if (first() != second()) { return 1; }; if (trace != 12) { return 2; }; return 42; };"),
	("short-circuit", "i32 calls; fn yes() -> bool { @calls = calls + 1; return true; }; fn no() -> bool { @calls = calls + 1; return false; }; fn skipped() -> bool { @calls = calls + 100; return true; }; fn main() -> i32 { if (no() && skipped()) { return 1; }; if (!(yes() || skipped())) { return 2; }; if (calls != 2) { return 3; }; return 42; };"),
	("caught-throw", "i32 calls; fn observed() -> bool { @calls = calls + 1; throw i32(7); }; fn main() -> i32 { try { if (observed()) { return 1; }; return 2; } catch (i32 caught) { if (calls != 1 || caught != 7) { return 3; }; }; return 42; };"),
	("known-normal-throw", "i32 calls; fn observed(bool fail) -> bool { @calls = calls + 1; if (fail) { throw i32(7); }; return true; }; fn inspect(bool fail) -> i32 { try { if (observed(fail)) { return 1; }; return 2; } catch (i32 caught) { if (calls != 1 || caught != 7) { return 3; }; }; return 42; }; fn main() -> i32 { return inspect(true); };"),
	("temporary-cleanup", "i32 destructions; struct Probe { fn ~Probe(var Probe& this) { @destructions = destructions + 1; }; }; fn observed(Probe& value) -> bool { return true; }; fn main() -> i32 { if (observed(Probe())) { if (destructions != 1) { return 1; }; }; if (destructions != 1) { return 2; }; return 42; };"),
	("call-counts", "i32 calls; fn observed() -> bool { @calls = calls + 1; return true; }; fn main() -> i32 { if (!observed()) { return 1; }; if (observed()) {}; if (calls != 2) { return 2; }; return 42; };"),
]


def run_native(directory, name):
	driver = directory / (name + ".cpp")
	driver.write_text("int condition_effect_anchor;\n", encoding="utf-8")
	executable = directory / (name + ".exe")
	command = directory / (name + ".cmd")
	command.write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/Tools/VsDevCmd.bat" -no_logo -arch=x64\n'
		+ 'if errorlevel 1 exit /b %errorlevel%\n'
		+ subprocess.list2cmdline(["cl", "/nologo", "/EHsc", "/MD", str(driver), "/Fo:" + str(directory / (name + "-driver.obj")),
			str(directory / (name + ".obj")), "/Fe:" + str(executable), "/link", "legacy_stdio_definitions.lib"])
		+ '\n', encoding="utf-8")
	linked = subprocess.run([str(command)], cwd=directory, capture_output=True, text=True, timeout=60)
	if linked.returncode:
		raise RuntimeError(linked.stdout + linked.stderr)
	result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
	if result.returncode != 42:
		raise RuntimeError(f"expected 42, got {result.returncode}\n{result.stdout}{result.stderr}")


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", type=Path, default=Path("build/release/native/rutilec.exe"))
	options = parser.parse_args()
	compiler = (ROOT / options.compiler).resolve()
	directory = ROOT / "build/compiler-new/condition-effects"
	directory.mkdir(parents=True, exist_ok=True)
	failures = []
	for name, source, function, comparisons, callee, calls in CODEGEN:
		try:
			functions = compile_case(compiler, directory, name, source)
			actual = len(re.findall(r"\bcmp\s", functions.get(function, "")))
			if function not in functions or actual != comparisons:
				raise RuntimeError(f"expected {comparisons} comparisons in {function}, got {actual}")
			relocations = (directory / (name + ".relocations.txt")).read_text(encoding="utf-8")
			actual = len(re.findall(r"\bREL32\b[^\n]*\b" + re.escape(callee) + r"\s*$", relocations, re.MULTILINE))
			if actual != calls:
				raise RuntimeError(f"expected {calls} calls to {callee}, got {actual}")
			print(name + "-codegen: PASS")
		except (OSError, RuntimeError, subprocess.SubprocessError) as error:
			failures.append(name)
			print(f"{name}: FAIL: {error}")
	for name, source in RUNTIME:
		try:
			compile_case(compiler, directory, name, source)
			run_native(directory, name)
			print(name + "-runtime: PASS")
		except (OSError, RuntimeError, subprocess.SubprocessError) as error:
			failures.append(name)
			print(f"{name}: FAIL: {error}")
	if failures:
		raise SystemExit(f"{len(failures)} condition-effect case(s) failed: {', '.join(failures)}")


if __name__ == "__main__":
	main()
