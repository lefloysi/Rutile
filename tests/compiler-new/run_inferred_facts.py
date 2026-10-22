"""Verify inferred integer facts diagnose bounds without assuming unknown values."""

import argparse
from pathlib import Path
import re
import subprocess

from run import run_case


ROOT = Path(__file__).resolve().parents[2]
BOUNDS = "array index is provably outside the array bounds"
CASES = [
	("local-invalid", "fn main() -> i32 { i32[2] values {}; usize index = 2; return values[index]; };", True),
	("local-valid", "fn main() -> i32 { i32[2] values {}; usize index = 1; return values[index]; };", False),
	("assignment-invalid", "fn main() -> i32 { i32[2] values {}; usize index = 0; @index = index + 2; return values[index]; };", True),
	("branch-invalid", "fn inspect(usize index) -> i32 { i32[2] values {}; if (index >= 2) { return values[index]; }; return 0; };", True),
	("branch-valid", "fn inspect(usize index) -> i32 { i32[2] values {}; if (index < 2) { return values[index]; }; return 0; };", False),
	("unknown", "fn inspect(usize index) -> i32 { i32[2] values {}; return values[index]; };", False),
	("signed-unknown", "fn inspect(i32 index) -> i32 { i32[2] values {}; if (index < 2) { return values[index]; }; return 0; };", False),
	("signed-negative-pointer", "fn inspect(var i32* values) -> i32 { i32 index = -1; return values[index]; };", False),
	("signed-negative-array", "fn main() -> i32 { i32[2] values {}; i32 index = -1; return values[index]; };", True),
	("signed-minimum-array", "fn main() -> i32 { i32[2] values {}; i64 index = -i64::max - 1; return values[index]; };", True),
	("signed-negative-branch", "fn inspect(i32 index) -> i32 { i32[2] values {}; if (index < 0) { return values[index]; }; return 0; };", True),
	("overflow", "fn main() -> i32 { i32[2] values {}; u08 index = 255; @index = index + u08(1); return values[index]; };", False),
	("loop-updates", "fn main() -> i32 { i32[2] values {}; usize index = 2; while (index) { @index = index - 1; values[index]; }; return 0; };", False),
	("opaque-call", "fn alter(var usize* index); fn main() -> i32 { i32[2] values {}; usize index = 2; alter(&@index); return values[index]; };", False),
	("unreachable", "fn main() -> i32 { i32[2] values {}; usize index = 2; if (false) { return values[index]; }; return 0; };", False),
	("narrowing-cast", "fn main() -> i32 { i32[2] values {}; u32 wide = 256; u08 index = static_cast<u08>(wide); return values[index]; };", False),
	("merge-all-invalid", "fn inspect(bool choice) -> i32 { i32[2] values {}; usize index; if (choice) { @index = 2; } else { @index = 3; }; return values[index]; };", True),
	("merge-mixed", "fn inspect(bool choice) -> i32 { i32[2] values {}; usize index; if (choice) { @index = 0; } else { @index = 2; }; return values[index]; };", False),
	("constructor-effects", "usize index = 2; struct Change { fn Change(var Change& this) { @index = 0; }; }; fn main() -> i32 { i32[2] values {}; Change change; return values[index]; };", False),
	("destructor-effects", "usize index = 2; struct Change { fn ~Change(var Change& this) { @index = 0; }; }; fn main() -> i32 { i32[2] values {}; { Change change; }; return values[index]; };", False),
	("overlapping-fields", "struct Inner { usize index; }; struct Outer { Inner inner; }; fn replace(var Inner* inner); fn main() -> i32 { i32[2] values {}; Outer owner; @owner.inner.index = 2; replace(&@owner.inner); return values[owner.inner.index]; };", False),
]

DEDUCTIONS = {
	"local-invalid": "inferred index range is [2, 2]; array extent is 2",
	"signed-negative-array": "inferred index range is [-1, -1]; array extent is 2",
	"signed-minimum-array": "inferred index range is [-9223372036854775808, -9223372036854775808]; array extent is 2",
	"merge-all-invalid": "inferred index range is [2, 3]; array extent is 2",
}


def check_codegen(compiler, directory):
	path = directory / "repeated-guard.rt"
	path.write_text("fn inspect(usize index, usize length) -> i32 { "
		"if (index >= length) { return 1; }; "
		"if (index >= length) { return 2; }; return 0; };\n", encoding="utf-8")
	output = directory / "repeated-guard.obj"
	subprocess.run([str(compiler), str(path), "--output", str(output),
		"--optimization", "0"], capture_output=True, text=True, check=True, timeout=30)
	toolsets = sorted(Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC").glob("*/bin/Hostx64/x64/dumpbin.exe"))
	if not toolsets:
		raise RuntimeError("dumpbin.exe unavailable for required code-generation verification")
	disassembly = subprocess.run([str(toolsets[-1]), "/nologo", "/disasm", str(output)],
		capture_output=True, text=True, check=True, timeout=30).stdout
	(directory / "repeated-guard.disasm.txt").write_text(disassembly, encoding="utf-8")
	comparisons = len(re.findall(r"\bcmp\s", disassembly))
	if comparisons != 1:
		raise RuntimeError(f"expected one comparison at optimization 0, found {comparisons}\n{disassembly}")
	print("repeated-guard-codegen: PASS")
	path = directory / "preserved-side-effects.rt"
	path.write_text("i32 calls; fn observe() -> bool { @calls = calls + 1; return true; }; "
		"fn main() -> i32 { if (observe()) {}; if (observe()) {}; return calls + 40; };\n", encoding="utf-8")
	run_case("facts-preserved-side-effects", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	path = directory / "signed-conversions.rt"
	path.write_text("fn main() -> i32 { i32 negative = -1; "
		"if (negative >= 0) { return 1; }; if (!(negative < 0)) { return 2; }; "
		"u32 source = 255; i08 narrowed = static_cast<i08>(source); "
		"if (narrowed != -1) { return 3; }; "
		"i32 lower = -2; if (lower >= negative) { return 4; }; return 42; };\n", encoding="utf-8")
	run_case("facts-signed-conversions", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	path = directory / "bool-conversion.rt"
	path.write_text("fn main() -> i32 { usize source = 2; bool value = static_cast<bool>(source); "
		"if (!value) { return 1; }; return 42; };\n", encoding="utf-8")
	run_case("facts-bool-conversion", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	path = directory / "narrowing-branch-provenance.rt"
	path.write_text("fn inspect(u32 input) -> i32 { "
		"if (static_cast<u08>(input) < 2) { if (input >= 256) { return 42; }; }; return 1; }; "
		"fn main() -> i32 { return inspect(256); };\n", encoding="utf-8")
	run_case("facts-narrowing-branch-provenance", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	path = directory / "address-and-versions.rt"
	path.write_text("fn compare(usize first, usize second) -> i32 { "
		"if (first < second) { @first = second; if (first < second) { return 1; }; }; return 42; }; "
		"fn main() -> i32 { i32 value = 1; var i32* pointer = &@value; @*pointer = 2; "
		"if (value != 2) { return 2; }; return compare(1, 3); };\n", encoding="utf-8")
	run_case("facts-address-and-versions", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	path = directory / "loop-value-versions.rt"
	path.write_text("fn main() -> i32 { usize index = 0; usize total = 0; "
		"while (index < 3) { @total = total + index; @index = index + 1; }; "
		"if (index != 3 || total != 3) { return 1; }; return 42; };\n", encoding="utf-8")
	run_case("facts-loop-value-versions", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	path = directory / "narrowing-exact-source.rt"
	path.write_text("fn main() -> i32 { usize number = 256; "
		"if (static_cast<u08>(number) < 2) { if (number != 256) { return 1; }; }; return 42; };\n", encoding="utf-8")
	run_case("facts-narrowing-exact-source", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	path = directory / "reference-member-alias.rt"
	path.write_text("struct Holder { var usize& value; }; fn main() -> i32 { usize number = 1; "
		"Holder holder = Holder(@number); @holder.value = 2; if (number != 2) { return 1; }; return 42; };\n", encoding="utf-8")
	run_case("facts-reference-member-alias", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", type=Path, default=Path("build/release/native/rutilec.exe"))
	options = parser.parse_args()
	compiler = (ROOT / options.compiler).resolve()
	directory = ROOT / "build/compiler-new/inferred-facts"
	directory.mkdir(parents=True, exist_ok=True)
	failures = []
	for name, source, rejected in CASES:
		path = directory / (name + ".rt")
		path.write_text(source + "\n", encoding="utf-8")
		try:
			result = subprocess.run([str(compiler), str(path), "--output",
				str(directory / (name + ".obj")), "--optimization", "0"],
				capture_output=True, text=True, timeout=30)
			output = result.stdout + result.stderr
			if (rejected and (result.returncode != 1 or BOUNDS not in output)
				or not rejected and result.returncode or "internal compiler error" in output):
				raise RuntimeError(f"unexpected exit {result.returncode}\n{output}")
			if name in DEDUCTIONS and DEDUCTIONS[name] not in output:
				raise RuntimeError(f"missing bounds deduction\n{output}")
			print(name + ": PASS")
		except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
			failures.append(name)
			print(f"{name}: FAIL: {error}")
	try:
		check_codegen(compiler, directory)
	except (OSError, RuntimeError, subprocess.SubprocessError) as error:
		failures.append("codegen-and-side-effects")
		print(f"codegen-and-side-effects: FAIL: {error}")
	if failures:
		raise SystemExit(f"{len(failures)} inferred-facts case(s) failed: {', '.join(failures)}")


if __name__ == "__main__":
	main()
