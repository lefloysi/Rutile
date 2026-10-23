"""Check lifetime scheduling through shared control-flow and storage events."""

import argparse
from pathlib import Path
import subprocess

from run import run_case


ROOT = Path(__file__).resolve().parents[2]
CASES = [
	("proven-branch-skips-move", "fn main() -> i32 { i32 value = 42; usize length = 0; if (length > 0) { $value; }; return value; };", None),
	("proven-branch-skips-dead-use", "fn main() -> i32 { i32 value = 42; $value; usize length = 0; if (length > 0) { return value; }; return 42; };", None),
	("proven-branch-skips-reference-use", "fn main() -> i32 { i32 value = 42; $value; usize length = 0; if (length > 0) { const i32& borrowed = value; return borrowed; }; return 42; };", None),
	("proven-false-loop", "fn main() -> i32 { i32 value = 42; usize length = 0; while (length) { $value; }; return value; };", None),
	("unknown-branch-still-merges", "fn inspect(usize length) -> i32 { i32 value = 42; if (length > 0) { $value; }; return value; };", "object lifetime differs across reachable control-flow paths"),
	("context-proof-not-generic", "fn inspect(usize length) -> i32 { i32 value = 42; if (length > 0) { $value; }; return value; }; fn main() -> i32 { return inspect(0); };", "object lifetime differs across reachable control-flow paths"),
	("opaque-call-forgets-condition", "fn alter(var usize* length); fn main() -> i32 { i32 value = 42; usize length = 0; alter(&@length); if (length > 0) { $value; }; return value; };", "object lifetime differs across reachable control-flow paths"),
	("widened-loop-still-checks", "fn main() -> i32 { i32 value = 42; usize index = 0; while (index < 2) { $value; @index = index + 1; }; return 42; };", "inconsistent object lifetimes at reachable loop backedge"),
	("widened-loop-late-move", "fn main() -> i32 { i32 value = 42; usize index = 0; while (index < 2) { if (index == 1) { $value; }; @index = index + 1; }; return value; };", "inconsistent object lifetimes at reachable loop backedge"),
	("condition-changes-with-value", "fn main() -> i32 { i32 value = 42; usize length = 0; if (length > 0) { $value; }; @length = 1; if (length > 0) { $value; }; return value; };", "object has been relocated"),
	("dead-branch-unused", "fn inspect(bool choice) -> i32 { i32 value = 42; if (choice) { $value; }; return 42; };", None),
	("dead-reason-order", "fn touch(); fn inspect(bool choice) -> i32 { i32 value = 42; if (choice) { touch(); touch(); touch(); $value; } else { @value.~i32(); }; return value; };", "object has been relocated"),
	("dead-reason-after-reconstruction", "fn inspect(bool choice) -> i32 { i32 value = 42; $value; if (choice) { @value.i32(1); @value.~i32(); }; return value; };", "object has been destroyed"),
	("reconstruct-merged", "fn inspect(bool choice) -> i32 { i32 value = 1; if (choice) { $value; } else { @value.~i32(); }; @value.i32(42); return value; };", None),
	("reconstruct-partial", "fn inspect(bool choice) -> i32 { i32 value = 1; if (choice) { $value; }; @value.i32(42); return value; };", None),
	("loop-restores", "fn inspect(bool choice) -> i32 { i32 value = 42; while (choice) { $value; @value.i32(42); }; return value; };", None),
	("nested-loop-restores", "fn inspect(bool outer, bool inner) -> i32 { i32 value = 42; while (outer) { while (inner) { $value; @value.i32(42); }; }; return value; };", None),
	("nested-loop-relocation", "fn consume(i32 value); fn inspect(bool outer, bool inner) -> i32 { i32 value = 42; while (outer) { while (inner) { consume($value); }; }; return 0; };", "inconsistent object lifetimes at reachable loop backedge"),
	("continue-relocation", "fn consume(i32 value); fn inspect(bool choice) -> i32 { i32 value = 42; for (; choice; ) { consume($value); continue; }; return 0; };", "inconsistent object lifetimes at reachable loop backedge"),
	("increment-continue-relocation", "fn consume(i32 value); fn inspect(bool choice) -> i32 { i32 value = 42; for (; choice; { consume($value); continue; }) { }; return 0; };", "inconsistent object lifetimes at reachable loop backedge"),
	("exception-reconstruction", "fn fail() -> i32 { throw i32(7); }; fn main() -> i32 { i32 value = 42; @value.~i32(); try { @value.i32(fail()); } catch (i32 caught) { return value; }; return 0; };", "object has been destroyed"),
	("reference-chain", "fn main() -> i32 { i32 value = 42; const i32& first = value; const i32& second = first; $value; return second; };", "object has been relocated"),
	("global-reference", "i32 global = 42; fn main() -> i32 { const i32& borrowed = global; return borrowed; };", None),
	("bypassed-reconstruction", "fn inspect(i32 choice) -> i32 { switch (choice) { case 0: i32 value = 1; case 1: @value.i32(42); return value; }; return 0; };", "object storage does not reach this construction"),
	("all-returns", "fn inspect(bool choice) -> i32 { if (choice) { return 42; }; return 42; };", None),
	("missing-return", "fn inspect(bool choice) -> i32 { if (choice) { return 42; }; };", "non-void function reaches the end without a result"),
	("constant-short-circuit", "fn main() -> i32 { i32 value = 42; false && bool($value); true || bool($value); return value; };", None),
]


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", type=Path, default=Path("build/release/native/rutilec.exe"))
	options = parser.parse_args()
	compiler = (ROOT / options.compiler).resolve()
	directory = ROOT / "build/compiler-new/graph-lifetimes"
	directory.mkdir(parents=True, exist_ok=True)
	failures = []
	for name, source, diagnostic in CASES:
		path = directory / (name + ".rt")
		path.write_text(source + "\n", encoding="utf-8")
		try:
			result = subprocess.run([str(compiler), str(path), "--output", str(directory / (name + ".obj"))], capture_output=True, text=True, timeout=30)
			output = result.stdout + result.stderr
			if diagnostic:
				if result.returncode != 1 or diagnostic not in output or "internal compiler error" in output:
					raise RuntimeError(output or f"unexpected exit {result.returncode}")
			elif result.returncode:
				raise RuntimeError(output)
			print(name + ": PASS")
		except (OSError, RuntimeError, subprocess.SubprocessError) as error:
			failures.append(name)
			print(f"{name}: FAIL: {error}")
	path = directory / "reconstruction-runtime.rt"
	path.write_text("fn main() -> i32 { i32 value = 1; @value.~i32(); @value.i32(42); usize index = 0; while (index < 2) { $value; @value.i32(42); @index = index + 1; }; return value; };\n", encoding="utf-8")
	try:
		run_case("graph-lifetime-reconstruction", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	except (OSError, RuntimeError, subprocess.SubprocessError) as error:
		failures.append("reconstruction-runtime")
		print(f"reconstruction-runtime: FAIL: {error}")
	path = directory / "elided-initializer-cleanup.rt"
	path.write_text("i32 destroyed; struct Probe { fn ~Probe(var Probe& this) { @destroyed = destroyed + 1; }; }; fn use(Probe& object) -> i32 { return 42; }; fn main() -> i32 { { Probe owned = Probe(); if (destroyed) { return 1; }; i32 answer = use(Probe()); if (answer != 42 || destroyed != 1) { return 2; }; }; if (destroyed != 2) { return 3; }; return 42; };\n", encoding="utf-8")
	try:
		run_case("graph-lifetime-elided-cleanup", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	except (OSError, RuntimeError, subprocess.SubprocessError) as error:
		failures.append("elided-cleanup-runtime")
		print(f"elided-cleanup-runtime: FAIL: {error}")
	path = directory / "proven-effectful-comparison.rt"
	path.write_text("i32 calls; fn observed() -> i32 { @calls = calls + 1; return 0; }; fn main() -> i32 { i32 value = 42; $value; if (observed() > 0) { return value; }; return calls + 41; };\n", encoding="utf-8")
	try:
		run_case("graph-lifetime-effectful-condition", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	except (OSError, RuntimeError, subprocess.SubprocessError) as error:
		failures.append("effectful-condition-runtime")
		print(f"effectful-condition-runtime: FAIL: {error}")
	path = directory / "proven-normal-condition-throws.rt"
	path.write_text("i32 calls; fn observed(bool fail) -> bool { @calls = calls + 1; if (fail) { throw i32(7); }; return true; }; fn inspect(bool fail) -> i32 { i32 value = 42; $value; try { if (observed(fail)) { return 42; }; return value; } catch (i32 caught) { if (caught != 7) { return 1; }; return 42; }; }; fn main() -> i32 { if (inspect(false) != 42 || inspect(true) != 42 || calls != 2) { return 2; }; return 42; };\n", encoding="utf-8")
	try:
		run_case("graph-lifetime-normal-condition-throws", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	except (OSError, RuntimeError, subprocess.SubprocessError) as error:
		failures.append("normal-condition-throws-runtime")
		print(f"normal-condition-throws-runtime: FAIL: {error}")
	if failures:
		raise SystemExit(f"{len(failures)} graph-lifetime case(s) failed: {', '.join(failures)}")


if __name__ == "__main__":
	main()
