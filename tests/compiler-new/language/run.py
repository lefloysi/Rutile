"""Compile the shared compiler analysis and exercise its native navigation test adapter."""

import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]


def checked(command, *, cwd=ROOT, environment=None, expected=0):
	result = subprocess.run(command, cwd=cwd, env=environment, capture_output=True, text=True, timeout=90)
	if result.returncode != expected:
		raise RuntimeError(f"Command returned {result.returncode}, expected {expected}:\n"
			+ (command if isinstance(command, str) else subprocess.list2cmdline(command)) + "\n" + result.stdout + result.stderr)
	return result


def main():
	arguments = argparse.ArgumentParser(description=__doc__)
	arguments.add_argument("--compiler", type=Path)
	arguments.add_argument("--build", default=".kiln/build/release")
	arguments.add_argument("--source-root", type=Path, default=ROOT)
	options = arguments.parse_args()
	build = ROOT / options.build
	compiler = (ROOT / options.compiler).resolve() if options.compiler else build / "target-636f6d70696c65722e657865/compiler.exe"
	runtime = "/MD"
	source_root = options.source_root.resolve()
	directory = ROOT / "build/compiler-new/language-shared"
	directory.mkdir(parents=True, exist_ok=True)
	standard = [source_root / "compiler/std" / (name + ".rt") for name in
		["unique", "optional", "buffer", "string_view", "string", "hash", "equal", "unordered_map"]]
	modules = sorted((source_root / "compiler/src").rglob("*.rt"))
	output = directory / "case.obj"
	checked([str(compiler), *map(str, standard + modules),
		str(ROOT / "tests/compiler-new/language/shared.rt"), "--output", str(output), "--optimization", "0"])
	print("language-shared: compiler source and fixture compile PASS", flush=True)

	native = ROOT.parent / "Kiln/build/bootstrap"
	toolchain = Path("C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/Tools/VsDevCmd.bat")
	executable = directory / "case.exe"
	command = ["cl", "/nologo", "/EHsc", runtime, "/std:c++20", "/I" + str(ROOT / "compiler/include"),
		str(ROOT / "tests/compiler-new/language/main.cpp"), str(output), str(ROOT / "base/src/windows.cpp"), str(ROOT / "base/src/runtime.cpp"),
		"/I" + str(ROOT / "base/include"), str(native / "lib/LLVM-C.lib"), "/Fe:" + str(executable), "/link", "legacy_stdio_definitions.lib",
		"kernel32.lib", "user32.lib", "shell32.lib", "advapi32.lib"]
	checked(f'cmd /d /s /c ""{toolchain}" -no_logo -arch=x64 && {subprocess.list2cmdline(command)}"', cwd=directory)
	environment = dict(os.environ)
	environment["PATH"] = str(native / "bin") + os.pathsep + environment.get("PATH", "")
	checked([str(executable)], cwd=directory, environment=environment, expected=42)
	replayed = (directory / "shared-target-diagnostic.txt").read_text()
	if "unsaved-error.rt:" not in replayed or "error:" not in replayed or "note:" not in replayed or "pick" not in replayed:
		raise RuntimeError("target diagnostic replay lost the saved primary, candidate note or source")
	print("language-shared: ABI, overloads, templates, leases, same-unit emission and diagnostics PASS")


if __name__ == "__main__":
	try:
		main()
	except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
		print(error, file=sys.stderr)
		sys.exit(1)
