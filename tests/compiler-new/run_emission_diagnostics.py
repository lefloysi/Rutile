"""Verify executable entry diagnostics through the compiler's executable C ABI."""

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def checked(command, directory, environment=None, expected=0):
	result = subprocess.run(command, cwd=directory, env=environment, capture_output=True,
		text=True, timeout=120)
	if result.returncode != expected:
		raise RuntimeError(f"Expected exit {expected}, got {result.returncode}:\n"
			+ result.stdout + result.stderr)
	return result


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--object", type=Path)
	parser.add_argument("--build", default=Path(".kiln/build/release"), type=Path)
	parser.add_argument("--compiler", type=Path)
	options = parser.parse_args()
	build = (ROOT / options.build).resolve()
	core = (ROOT / options.object).resolve() if options.object else build / "target-727574696c652d6c616e67756167652e6f626a/rutile-language.obj"
	runtime = "/MD"
	directory = build / "tests/emission-diagnostics"
	directory.mkdir(parents=True, exist_ok=True)
	native = ROOT.parent / "Kiln/build/bootstrap"
	toolchain = Path("C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/Tools/VsDevCmd.bat")
	executable = directory / "entry_diagnostics.exe"
	command = ["cl", "/nologo", "/EHsc", runtime, "/std:c++20",
		str(ROOT / "tests/compiler-new/backend/entry_diagnostics.cpp"), str(core),
		str(ROOT / "base/src/windows.cpp"), str(ROOT / "base/src/runtime.cpp"),
		"/I" + str(ROOT / "base/include"), str(native / "lib/LLVM-C.lib"),
		"/Fe:" + str(executable), "/link", "legacy_stdio_definitions.lib",
		"kernel32.lib", "user32.lib", "shell32.lib", "advapi32.lib"]
	checked(f'cmd /d /s /c ""{toolchain}" -no_logo -arch=x64 && {subprocess.list2cmdline(command)}"', directory)
	environment = dict(os.environ)
	environment["PATH"] = str(native / "bin") + os.pathsep + environment.get("PATH", "")

	cases = [
		("missing", "fn helper() -> i32 { return 42; };", "absent_entry", ["absent_entry", "not declared|unknown"]),
		("global", "var global_entry: i32 = 42;", "global_entry", ["global_entry", "global variable"]),
		("declaration", "fn declared_entry() -> i32;", "declared_entry", ["declared_entry", "without a definition|not defined", "i32"]),
		("parameters", "fn parameter_entry(i32 value) -> i32 { return value; };", "parameter_entry", ["parameter_entry", "parameter", "i32"]),
		("result", "fn floating_entry() -> f32 { return 1.0; };", "floating_entry", ["floating_entry", "f32", "result"]),
	]
	failures = []
	for name, source, entry, patterns in cases:
		path = directory / (name + ".rt")
		path.write_text(source + "\n")
		try:
			result = checked([str(executable), str(path), str(directory / (name + ".obj")), entry], directory, environment, 1)
			text = result.stdout + result.stderr
			for pattern in patterns:
				if not re.search(pattern, text, re.IGNORECASE):
					raise RuntimeError(f"{name}: missing diagnostic context {pattern!r}\n{text}")
			print(f"emission-{name}: PASS", flush=True)
		except RuntimeError as error:
			failures.append(name)
			print(error, file=sys.stderr)

	path = directory / "custom.rt"
	path.write_text("fn custom_entry() -> i32 { return 42; };\n")
	output = directory / "custom.obj"
	checked([str(executable), str(path), str(output), "custom_entry"], directory, environment)
	anchor = directory / "anchor.cpp"
	anchor.write_text("int entry_test_anchor = 0;\n")
	program = directory / "custom.exe"
	command = ["cl", "/nologo", "/EHsc", runtime, str(anchor), str(output),
		"/Fe:" + str(program), "/link", "legacy_stdio_definitions.lib"]
	checked(f'cmd /d /s /c ""{toolchain}" -no_logo -arch=x64 && {subprocess.list2cmdline(command)}"', directory)
	checked([str(program)], directory, environment, 42)
	print("emission-custom-entry: PASS", flush=True)

	if options.compiler:
		path = directory / "fallthrough.rt"
		path.write_text("fn unfinished_result() -> i32 { };\n")
		result = checked([str((ROOT / options.compiler).resolve()), str(path), "--output",
			str(directory / "fallthrough.obj")], directory, environment, 1)
		text = result.stdout + result.stderr
		if "unfinished_result" not in text or "i32" not in text:
			raise RuntimeError("fallthrough diagnostic lost function/result context:\n" + text)
		print("emission-fallthrough: PASS", flush=True)

		path = directory / "output-failure.rt"
		path.write_text("fn main() { };\n")
		output_directory = directory / "requested-output-directory"
		output_directory.mkdir(exist_ok=True)
		result = checked([str((ROOT / options.compiler).resolve()), str(path), "--output",
			str(output_directory)], directory, environment, 1)
		text = result.stdout + result.stderr
		requested_path = str(output_directory)
		path_present = requested_path in text or requested_path.replace("\\", "\\\\") in text
		if not path_present or not re.search(r"cannot open", text, re.IGNORECASE):
			raise RuntimeError("output-open diagnostic lost requested path or failure context:\n" + text)
		print("emission-output-open: PASS", flush=True)

	count = 8 if options.compiler else 6
	print(f"emission diagnostics: {count - len(failures)} passed, {len(failures)} failed")
	return bool(failures)


if __name__ == "__main__":
	try:
		sys.exit(main())
	except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
		print(error, file=sys.stderr)
		sys.exit(1)
