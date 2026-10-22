"""Compile, link, and execute an isolated new-compiler component against the bootstrap."""

import argparse
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]


def run_case(name, sources, entry="main", expected=42, reject=None, compiler=None):
	directory = ROOT / "build" / "compiler-new" / name
	directory.mkdir(parents=True, exist_ok=True)
	compiler = ROOT / (compiler or ".kiln/build/release/target-727574696c65632e657865/rutilec.exe")
	output = directory / "case.obj"
	command = [str(compiler), *[str(ROOT / source) for source in sources],
		"--output", str(output), "--optimization", "2"]
	compiled = subprocess.run(command, capture_output=True, text=True)
	if reject:
		if compiled.returncode != 1 or not re.search(reject, compiled.stdout + compiled.stderr):
			raise RuntimeError(f"{name}: expected diagnostic {reject!r}\n{compiled.stdout}{compiled.stderr}")
		print(f"{name}: PASS")
		return
	if compiled.returncode:
		raise RuntimeError(f"{name}: compile failed\n{compiled.stdout}{compiled.stderr}")
	driver = directory / "driver.cpp"
	if entry == "main":
		driver.write_text("int rutile_test_anchor = 0;\n")
	else:
		if not re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]*", entry):
			raise ValueError("Invalid test entry")
		driver.write_text(f'extern "C" int {entry}();\nint main() {{ return {entry}(); }}\n')
	executable = directory / "case.exe"
	toolchain = Path("C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/Tools/VsDevCmd.bat")
	link = subprocess.list2cmdline(["cl", "/nologo", "/EHsc", "/MD", str(driver),
		str(output), "/Fe:" + str(executable), "/link", "legacy_stdio_definitions.lib"])
	linked = subprocess.run(f'cmd /d /s /c ""{toolchain}" -no_logo -arch=x64 && {link}"', cwd=directory,
		capture_output=True, text=True)
	if linked.returncode:
		raise RuntimeError(f"{name}: link failed\n{linked.stdout}{linked.stderr}")
	result = subprocess.run([str(executable)], capture_output=True, text=True)
	if result.returncode != expected:
		raise RuntimeError(f"{name}: expected {expected}, got {result.returncode}\n{result.stdout}{result.stderr}")
	print(f"{name}: PASS")


if __name__ == "__main__":
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--name", required=True)
	parser.add_argument("--entry", default="main")
	parser.add_argument("--expect", type=int, default=42)
	parser.add_argument("--reject")
	parser.add_argument("--compiler")
	parser.add_argument("sources", nargs="+")
	args = parser.parse_args()
	try:
		run_case(args.name, args.sources, args.entry, args.expect, args.reject, args.compiler)
	except (OSError, RuntimeError, ValueError) as error:
		print(error, file=sys.stderr)
		sys.exit(1)
