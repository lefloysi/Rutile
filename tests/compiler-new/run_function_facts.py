"""Check inferred function facts across actual call boundaries."""

import argparse
from pathlib import Path
import re
import subprocess

from run import run_case


ROOT = Path(__file__).resolve().parents[2]
BOUNDS = "array index is provably outside the array bounds"
CASES = [
	("guarded-shrink-invalid", "fn shrink(var usize& value) -> bool { if (value == 0) { return false; }; @value = value - 1; return true; }; fn main() -> i32 { i32[2] data {}; usize index = 3; if (shrink(@index)) { return data[index]; }; return 0; };", True),
	("assignment-address-after-value", "fn main() -> i32 { i32[2] data {}; usize index = 2; @data[index] = i32(@index = 0) + 42; return data[0]; };", False),
	("assignment-value-invalidates-address", "fn main() -> i32 { i32[2] data {}; usize index = 0; @data[index] = i32(@index = 2) + 40; return 42; };", True),
	("array-element-preserves-scalar", "fn main() -> i32 { i32[2] data {}; usize index = 2; @data[0] = 7; return data[index]; };", True),
	("array-unknown-element-preserves-scalar", "fn inspect(usize element) -> i32 { i32[2] data {}; usize index = 2; if (element < 2) { @data[element] = 7; return data[index]; }; return 0; };", True),
	("array-unproven-element-unknown", "fn inspect(usize element) -> i32 { i32[2] data {}; usize index = 2; @data[element] = 7; return data[index]; };", False),
	("array-element-preserves-sibling", "struct Holder { usize length; i32[2] data; }; fn main() -> i32 { i32[2] data {}; Holder holder; @holder.length = 2; @holder.data[0] = 7; return data[holder.length]; };", True),
	("raw-element-write-unknown", "fn main() -> i32 { i32[2] data {}; var i32* pointer = &@data[0]; usize index = 2; @pointer[0] = 7; return data[index]; };", False),
	("relocated-value-facts", "fn main() -> i32 { i32[2] data {}; usize index = 2; usize moved = $index; return data[moved]; };", True),
	("relocated-value-snapshot", "fn main() -> i32 { i32[2] data {}; usize index = 2; usize moved = $index; @index.usize(0); return data[moved]; };", True),
	("unknown-readonly-arithmetic", "fn multiply(usize value) -> usize { return value * value; }; fn inspect(usize value) -> i32 { i32[2] data {}; usize index = 2; multiply(value); return data[index]; };", True),
	("unknown-arithmetic-result", "fn multiply(usize value) -> usize { return value * value; }; fn inspect(usize value) -> i32 { i32[2] data {}; return data[multiply(value)]; };", False),
	("unknown-arithmetic-effects", "fn multiply(var usize& value) -> usize { return (@value = 0) * 2; }; fn main() -> i32 { i32[2] data {}; usize index = 2; multiply(@index); return data[index]; };", False),
	("throw-is-program-outcome", "fn checked(usize index) -> usize { if (index >= 2) { throw i32(7); }; return index; }; fn main() -> i32 { return i32(checked(3)); };", False),
	("normal-return-bounds", "fn checked(usize index) -> usize { if (index < 2) { throw i32(7); }; return index; }; fn inspect(usize index) -> i32 { i32[2] data {}; usize result = checked(index); return data[result]; };", True),
	("normal-return-wrapper", "fn checked(usize index) -> usize { if (index < 2) { throw i32(7); }; return index; }; fn wrapper(usize index) -> usize { return checked(index); }; fn inspect(usize index) -> i32 { i32[2] data {}; return data[wrapper(index)]; };", True),
	("normal-void-wrapper", "fn checked(usize index) { if (index < 2) { throw i32(7); }; }; fn wrapper(usize index) { checked(index); }; fn inspect(usize index) -> i32 { i32[2] data {}; wrapper(index); return data[index]; };", True),
	("caught-throw-effects", "fn reset(var usize& index) { @index = 0; throw i32(7); }; fn main() -> i32 { i32[2] data {}; usize index = 2; try { reset(@index); } catch (i32 caught) {}; return data[index]; };", False),
	("read-return-unknown", "fn checked(var usize* data, usize index) -> usize { if (index >= 2) { throw i32(7); }; return data[index]; }; fn inspect(var usize* data, usize index) -> i32 { i32[2] fixed {}; return fixed[checked(data, index)]; };", False),
	("identity-invalid", "fn identity(usize value) -> usize { return value; }; fn main() -> i32 { i32[2] values {}; return values[identity(2)]; };", True),
	("identity-valid", "fn identity(usize value) -> usize { return value; }; fn main() -> i32 { i32[2] values {}; return values[identity(1)]; };", False),
	("field-getter-invalid", "struct Counter { usize length; }; fn read(Counter& counter) -> usize { return counter.length; }; fn main() -> i32 { i32[2] values {}; Counter counter; @counter.length = 2; return values[read(counter)]; };", True),
	("field-mutator-invalid", "struct Counter { usize length; }; fn increase(var Counter& counter) { @counter.length = counter.length + 1; }; fn read(Counter& counter) -> usize { return counter.length; }; fn main() -> i32 { i32[2] values {}; Counter counter; @counter.length = 1; increase(@counter); return values[read(counter)]; };", True),
	("conditional-return-invalid", "fn select(usize value) -> usize { if (value < 2) { return 0; }; return 3; }; fn main() -> i32 { i32[2] values {}; return values[select(4)]; };", True),
	("conditional-return-valid", "fn select(usize value) -> usize { if (value < 2) { return 0; }; return 3; }; fn main() -> i32 { i32[2] values {}; return values[select(1)]; };", False),
	("guarded-write-invalid", "fn increase(var usize& value) -> bool { if (value < 3) { @value = value + 1; return true; }; return false; }; fn main() -> i32 { i32[2] values {}; usize value = 1; increase(@value); return values[value]; };", True),
	("opaque-pointer-write", "fn alter(var usize* value) { @*value = 0; }; fn main() -> i32 { i32[2] values {}; usize index = 2; alter(&@index); return values[index]; };", False),
	("multiple-return-paths", "fn inspect(usize index, usize length) -> i32 { if (index >= length) { return 1; }; if (index >= length) { return 2; }; return 0; };", False),
	("scalar-wrapper-invalid", "fn identity(usize value) -> usize { return value; }; fn wrap(usize value) -> usize { return identity(value); }; fn main() -> i32 { i32[2] values {}; return values[wrap(2)]; };", True),
	("reverse-wrapper-invalid", "fn wrap(usize value) -> usize { return identity(value); }; fn identity(usize value) -> usize { return value; }; fn main() -> i32 { i32[2] values {}; return values[wrap(2)]; };", True),
	("field-wrapper-invalid", "struct Counter { usize length; }; fn read(Counter& counter) -> usize { return counter.length; }; fn wrap(Counter& counter) -> usize { return read(counter); }; fn main() -> i32 { i32[2] values {}; Counter counter; @counter.length = 2; return values[wrap(counter)]; };", True),
	("recursive-wrapper-unknown", "fn recursive(usize value) -> usize { return recursive(value); }; fn inspect(usize value) -> i32 { i32[2] values {}; return values[recursive(value)]; };", False),
	("bool-result-state-invalid", "fn set(var usize& value, bool choice) -> bool { if (choice) { @value = 3; return true; }; return false; }; fn inspect(bool choice) -> i32 { i32[2] values {}; usize index = 1; bool result = set(@index, choice); if (result) { return values[index]; }; return 0; };", True),
	("bool-result-other-state-valid", "fn set(var usize& value, bool choice) -> bool { if (choice) { @value = 3; return true; }; return false; }; fn inspect(bool choice) -> i32 { i32[2] values {}; usize index = 1; bool result = set(@index, choice); if (!result) { return values[index]; }; return 0; };", False),
	("bool-result-state-merged", "fn set(var usize& value, bool choice) -> bool { if (choice) { @value = 3; return true; }; return false; }; fn inspect(bool choice) -> i32 { i32[2] values {}; usize index = 1; set(@index, choice); return values[index]; };", False),
	("bool-result-state-overwritten", "fn set(var usize& value, bool choice) -> bool { if (choice) { @value = 3; return true; }; return false; }; fn inspect(bool choice) -> i32 { i32[2] values {}; usize index = 1; bool result = set(@index, choice); @index = 0; if (result) { return values[index]; }; return 0; };", False),
	("union-result-state-invalid", "using Choice = i32 | i64; fn set(var usize& value, bool choice) -> Choice { if (choice) { @value = 3; return Choice(i32(0)); }; return Choice(i64(0)); }; fn inspect(bool choice) -> i32 { i32[2] values {}; usize index = 1; Choice result = set(@index, choice); if (result.has<i32>()) { return values[index]; }; return 0; };", True),
	("union-result-other-state-valid", "using Choice = i32 | i64; fn set(var usize& value, bool choice) -> Choice { if (choice) { @value = 3; return Choice(i32(0)); }; return Choice(i64(0)); }; fn inspect(bool choice) -> i32 { i32[2] values {}; usize index = 1; Choice result = set(@index, choice); if (result.has<i64>()) { return values[index]; }; return 0; };", False),
	("union-result-state-merged", "using Choice = i32 | i64; fn set(var usize& value, bool choice) -> Choice { if (choice) { @value = 3; return Choice(i32(0)); }; return Choice(i64(0)); }; fn inspect(bool choice) -> i32 { i32[2] values {}; usize index = 1; Choice result = set(@index, choice); return values[index]; };", False),
	("union-empty-record-state", "struct First {}; struct Other {}; using Choice = First | Other; fn set(var usize& value, bool choice) -> Choice { if (choice) { @value = 3; return Choice(First()); }; return Choice(Other()); }; fn inspect(bool choice) -> i32 { i32[2] values {}; usize index = 1; Choice result = set(@index, choice); if (result.has<First>()) { return values[index]; }; return 0; };", True),
	("union-payload-effects-unknown", "usize index; struct First { fn First(var First& this) { @index = 0; }; }; using Choice = First | i32; fn create() -> Choice { return Choice(First()); }; fn main() -> i32 { i32[2] values {}; @index = 2; Choice result = create(); return values[index]; };", False),
	("numeric-constructor-return", "fn identity(usize value) -> usize { return usize(value); }; fn main() -> i32 { i32[2] values {}; return values[identity(2)]; };", True),
	("deep-reverse-wrappers", " ".join(f"fn wrap{index}(usize value) -> usize {{ return wrap{index + 1}(value); }};" for index in range(8)) + " fn wrap8(usize value) -> usize { return value; }; fn main() -> i32 { i32[2] values {}; return values[wrap0(2)]; };", True),
	("virtual-target-unknown", "struct Base { fn index(Base& this) -> usize : [virtual] { return 2; }; }; struct Child : [Base] { fn index(Child& this) -> usize : [override] { return 0; }; }; fn inspect(Base& object) -> i32 { i32[2] values {}; return values[object.index()]; };", False),
	("switch-preserves-facts", "fn inspect(usize choice) -> i32 { i32[2] values {}; usize index = 2; switch (choice) { case 0: break; default: break; }; return values[index]; };", True),
	("switch-merge-invalid", "fn inspect(usize choice) -> i32 { i32[2] values {}; usize index = 0; switch (choice) { case 0: @index = 2; break; default: @index = 3; }; return values[index]; };", True),
	("switch-missing-default", "fn inspect(usize choice) -> i32 { i32[2] values {}; usize index = 1; switch (choice) { case 0: @index = 2; break; }; return values[index]; };", False),
	("switch-fallthrough", "fn main() -> i32 { i32[2] values {}; usize index = 2; switch (0) { case 0: @index = 3; case 1: @index = 0; break; default: @index = 3; }; return values[index]; };", False),
	("switch-constant-break", "fn main() -> i32 { i32[2] values {}; usize index = 0; switch (0) { case 0: @index = 2; break; default: @index = 0; }; return values[index]; };", True),
	("try-normal-preserves-facts", "fn main() -> i32 { i32[2] values {}; usize index = 2; try { @index = 3; } catch (i32 caught) { @index = 0; }; return values[index]; };", True),
	("try-catch-merge-invalid", "fn inspect(bool choice) -> i32 { i32[2] values {}; usize index = 0; try { if (choice) { throw i32(1); }; @index = 3; } catch (i32 caught) { @index = 3; }; return values[index]; };", True),
	("try-typed-handlers-conservative", "fn main() -> i32 { i32[2] values {}; usize index = 0; try { throw i32(1); } catch (i32 first) { @index = 3; } catch (i64 second) { @index = 0; }; return values[index]; };", False),
	("try-handler-rethrow-outer", "fn main() -> i32 { i32[2] values {}; usize index = 0; try { try { throw i32(1); } catch (i32 first) { throw; }; } catch (i32 second) { @index = 3; }; return values[index]; };", True),
	("scope-destructor-effects", "usize index; struct Change { fn ~Change(var Change& this) { @index = 0; }; }; fn main() -> i32 { i32[2] values {}; { Change change; @index = 2; }; return values[index]; };", False),
	("break-destructor-effects", "usize index; struct Change { fn ~Change(var Change& this) { @index = 0; }; }; fn main() -> i32 { i32[2] values {}; while (true) { Change change; @index = 2; break; }; return values[index]; };", False),
	("continue-destructor-effects", "usize index; struct Change { fn ~Change(var Change& this) { @index = 0; }; }; fn main() -> i32 { i32[2] values {}; usize count = 0; while (count < 1) { Change change; @index = 2; @count = count + 1; continue; }; return values[index]; };", False),
	("local-reference-alias-effects", "fn reset(var usize& value) { var usize& alias = @value; @alias = 0; }; fn main() -> i32 { i32[2] values {}; usize index = 2; reset(@index); return values[index]; };", False),
	("reference-field-alias-effects", "struct Holder { var usize& reference; }; fn reset(var Holder& holder) { @holder.reference = 0; }; fn main() -> i32 { i32[2] values {}; usize index = 2; Holder holder = Holder(@index); reset(@holder); return values[index]; };", False),
	("temporary-statement-effects", "usize index; struct Changer { fn ~Changer(var Changer& this) { @index = 0; }; }; fn pure(Changer& object, usize value) -> usize { return value; }; fn main() -> i32 { i32[2] values {}; pure(Changer(), (@index = 2)); return values[index]; };", False),
	("temporary-condition-effects", "usize index; struct Changer { fn ~Changer(var Changer& this) { @index = 0; }; }; fn pure(Changer& object, usize value) -> usize { return value; }; fn main() -> i32 { i32[2] values {}; if (pure(Changer(), (@index = 2))) {}; return values[index]; };", False),
	("temporary-loop-effects", "usize index; struct Changer { fn ~Changer(var Changer& this) { @index = 0; }; }; fn pure(Changer& object, usize value) -> usize { return value; }; fn main() -> i32 { i32[2] values {}; while (pure(Changer(), (@index = 2))) { break; }; return values[index]; };", False),
	("temporary-switch-effects", "usize index; struct Changer { fn ~Changer(var Changer& this) { @index = 0; }; }; fn pure(Changer& object, usize value) -> usize { return value; }; fn main() -> i32 { i32[2] values {}; switch (pure(Changer(), (@index = 2))) { case 2: break; default: break; }; return values[index]; };", False),
	("temporary-logical-effects", "usize index; struct Changer { fn ~Changer(var Changer& this) { @index = 0; }; }; fn pure(Changer& object, usize value) -> usize { return value; }; fn main() -> i32 { i32[2] values {}; if (pure(Changer(), (@index = 2)) && true) {}; return values[index]; };", False),
	("temporary-call-result-effects", "usize index; struct Changer { fn ~Changer(var Changer& this) { @index = 0; }; }; fn pure(Changer& object, usize value) -> usize { return value; }; fn create() -> Changer { return Changer(); }; fn main() -> i32 { i32[2] values {}; pure(create(), (@index = 2)); return values[index]; };", False),
]

AFFINE_SOURCE = """
fn grow(var usize& value) -> bool {
	if (value == usize::max) { return false; };
	@value = value + 1;
	return true;
};
fn shrink(var usize& value) -> bool {
	if (value == 0) { return false; };
	@value = value - 1;
	return true;
};
fn verify(usize value) -> bool {
	usize old = value;
	bool changed = grow(@value);
	if (changed) { return value > old; };
	return true;
};
fn local(usize value) -> bool {
	if (value < 100) {
		usize grown = value + 1;
		return !(grown <= value);
	};
	return true;
};
fn uncertain(usize value) -> bool {
	usize grown = value + 1;
	return grown > value;
};
fn checked(usize index, usize length) -> bool {
	if (index >= length) { return false; };
	return true;
};
fn guard(usize index, usize length) -> bool {
	if (checked(index, length)) { return index < length; };
	return true;
};
fn loose(usize index, usize length, bool bypass) -> bool {
	if (bypass) { return true; };
	if (index >= length) { return false; };
	return true;
};
fn guard_loose(usize index, usize length, bool bypass) -> bool {
	if (loose(index, length, bypass)) { return index < length; };
	return true;
};
fn replace(var usize& value, usize replacement) { @value = replacement; };
fn guard_changed(usize index, usize length, usize replacement) -> bool {
	if (checked(index, length)) {
		replace(@index, replacement);
		return index < length;
	};
	return true;
};
struct Counter { usize length; };
fn read(Counter& counter) -> usize { return counter.length; };
fn getter_identity(usize value) -> bool {
	Counter counter;
	usize snapshot = value;
	@counter.length = snapshot;
	if (read(counter) == snapshot) { return true; };
	return false;
};
fn getter_growth(usize value) -> bool {
	Counter counter;
	usize snapshot = value;
	@counter.length = snapshot;
	if (grow(@counter.length)) {
		if (read(counter) > snapshot) { return true; };
		return false;
	};
	return true;
};
fn getter_shrink(usize value) -> bool {
	Counter counter;
	usize snapshot = value;
	@counter.length = snapshot;
	if (shrink(@counter.length)) {
		if (read(counter) < snapshot) { return true; };
		return false;
	};
	return true;
};
fn main() -> i32 {
	if (!verify(0) || !verify(usize::max - 1) || !verify(usize::max)) { return 1; };
	if (!local(0) || !local(99) || !local(100)) { return 2; };
	if (uncertain(usize::max)) { return 3; };
	if (!guard(0, 2) || !guard(3, 2)) { return 4; };
	if (guard_loose(3, 2, true)) { return 5; };
	if (guard_changed(0, 2, 3)) { return 6; };
	if (!getter_identity(0) || !getter_identity(usize::max)) { return 7; };
	if (!getter_growth(0) || !getter_growth(usize::max)) { return 8; };
	if (!getter_shrink(0) || !getter_shrink(usize::max)) { return 9; };
	return 42;
};
"""


def check_affine_codegen(compiler, directory, function, expected):
	path = directory / "affine-growth.rt"
	path.write_text(AFFINE_SOURCE, encoding="utf-8")
	output = directory / "affine-growth.obj"
	subprocess.run([str(compiler), str(path), "--output", str(output),
		"--optimization", "0"], capture_output=True, text=True, check=True, timeout=30)
	toolsets = sorted(Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC").glob("*/bin/Hostx64/x64/dumpbin.exe"))
	if not toolsets:
		raise RuntimeError("dumpbin.exe unavailable for required code-generation verification")
	disassembly = subprocess.run([str(toolsets[-1]), "/nologo", "/disasm", str(output)],
		capture_output=True, text=True, check=True, timeout=30).stdout
	(directory / "affine-growth.disasm.txt").write_text(disassembly, encoding="utf-8")
	match = re.search(rf"(?m)^{re.escape(function)}:\s*\n", disassembly)
	if not match:
		raise RuntimeError(f"missing disassembly for {function}")
	body = re.split(r"(?m)^[^\s:]+:\s*$|^\s+Summary\s*$", disassembly[match.end():], maxsplit=1)[0]
	# LLVM may emit subtraction plus SETcc instead of CMP for a returned bool.
	comparisons = len(re.findall(r"\b(?:cmp|set[a-z]+)\s", body))
	retained = function in ("uncertain", "guard_loose", "guard_changed")
	if (retained and not comparisons or not retained and comparisons != expected):
		raise RuntimeError(f"expected {expected} comparisons in {function} at optimization 0, found {comparisons}\n{body}")
	print(f"affine-{function}-codegen: PASS")


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", type=Path, default=Path("build/release/native/rutilec.exe"))
	options = parser.parse_args()
	compiler = (ROOT / options.compiler).resolve()
	directory = ROOT / "build/compiler-new/function-facts"
	directory.mkdir(parents=True, exist_ok=True)
	failures = []
	for name, source, rejected in CASES:
		path = directory / (name + ".rt")
		path.write_text(source + "\n", encoding="utf-8")
		try:
			result = subprocess.run([str(compiler), str(ROOT / "compiler/std/variant.rt"), str(path), "--output",
				str(directory / (name + ".obj")), "--optimization", "0"],
				capture_output=True, text=True, timeout=30)
			output = result.stdout + result.stderr
			if rejected:
				if result.returncode != 1 or BOUNDS not in output:
					raise RuntimeError(f"expected bounds diagnostic; exit {result.returncode}\n{output}")
			elif result.returncode:
				raise RuntimeError(f"unexpected exit {result.returncode}\n{output}")
			print(name + ": PASS")
		except (OSError, RuntimeError, subprocess.SubprocessError) as error:
			failures.append(name)
			print(f"{name}: FAIL: {error}")
	path = directory / "runtime-mutation.rt"
	path.write_text("fn increase(var usize& value) -> bool { if (value < 42) { "
		"@value = value + 1; return true; }; return false; }; "
		"fn main() -> i32 { usize value = 41; increase(@value); return i32(value); };\n",
		encoding="utf-8")
	try:
		run_case("facts-runtime-mutation", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	except (OSError, RuntimeError, subprocess.SubprocessError) as error:
		failures.append("runtime-mutation")
		print(f"runtime-mutation: FAIL: {error}")
	for function, expected in [("verify", 0), ("local", 1), ("uncertain", 1),
		("guard", 0), ("guard_loose", 1), ("guard_changed", 1),
		("getter_identity", 0), ("getter_growth", 0), ("getter_shrink", 0)]:
		try:
			check_affine_codegen(compiler, directory, function, expected)
		except (OSError, RuntimeError, subprocess.SubprocessError) as error:
			failures.append(f"affine-{function}-codegen")
			print(f"affine-{function}-codegen: FAIL: {error}")
	try:
		run_case("facts-affine-runtime", [(directory / "affine-growth.rt").relative_to(ROOT).as_posix()], compiler=str(compiler))
	except (OSError, RuntimeError, subprocess.SubprocessError) as error:
		failures.append("affine-runtime")
		print(f"affine-runtime: FAIL: {error}")
	path = directory / "cleanup-runtime.rt"
	path.write_text("""
usize index;
i32 destructions;
struct Change {
	fn ~Change(var Change& this) { @index = 0; @destructions = destructions + 1; };
};
fn before_return() -> usize { Change change; @index = 2; return index; };
fn pure(Change& object, usize value) -> usize { return value; };
fn with_temporary() -> usize { return pure(Change(), (@index = 2)); };
fn main() -> i32 {
	{ Change change; @index = 2; };
	if (index != 0 || destructions != 1) { return 1; };
	usize captured = before_return();
	if (captured != 2 || index != 0 || destructions != 2) { return 2; };
	while (true) { Change change; @index = 2; break; };
	if (index != 0 || destructions != 3) { return 3; };
	usize iterations = 0;
	while (iterations < 2) {
		Change change; @index = 2; @iterations = iterations + 1; continue;
	};
	if (index != 0 || destructions != 5) { return 4; };
	try { Change change; @index = 2; throw i32(7); }
	catch (i32 caught) { if (index != 0 || destructions != 6) { return 5; }; };
	usize snapshot = pure(Change(), (@index = 2));
	if (snapshot != 2 || index != 0 || destructions != 7) { return 6; };
	usize returned = with_temporary();
	if (returned != 2 || index != 0 || destructions != 8) { return 7; };
	return 42;
};
""", encoding="utf-8")
	try:
		run_case("facts-cleanup-runtime", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	except (OSError, RuntimeError, subprocess.SubprocessError) as error:
		failures.append("cleanup-runtime")
		print(f"cleanup-runtime: FAIL: {error}")
	path = directory / "float-unordered-runtime.rt"
	path.write_text("fn equal(f64 value) -> bool { return value == value; }; "
		"fn ordered(f64 value) -> bool { return value <= value; }; "
		"fn identity(f64 value) -> f64 { return value; }; "
		"fn wrapped(f64 value) -> bool { return identity(value) == identity(value); }; "
		"fn main() -> i32 { f64 zero = 0.0; f64 value = zero / zero; "
		"if (equal(value) || ordered(value) || wrapped(value)) { return 1; }; "
		"return 42; };\n", encoding="utf-8")
	try:
		run_case("facts-float-unordered-runtime", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	except (OSError, RuntimeError, subprocess.SubprocessError) as error:
		failures.append("float-unordered-runtime")
		print(f"float-unordered-runtime: FAIL: {error}")
	if failures:
		raise SystemExit(f"{len(failures)} function-facts case(s) failed: {', '.join(failures)}")


if __name__ == "__main__":
	main()
