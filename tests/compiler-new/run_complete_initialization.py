"""Verify complete construction avoids clobbering its source or pre-clearing its destination."""

import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def checked(command, directory, expected=0):
	result = subprocess.run(command, cwd=directory, capture_output=True, text=True, timeout=120)
	if result.returncode != expected:
		raise RuntimeError(f"Expected exit {expected}, got {result.returncode}:\n"
			+ result.stdout + result.stderr)
	return result


def native(command, directory, toolchain):
	return checked(f'cmd /d /s /c ""{toolchain}" -no_logo -arch=x64 && {subprocess.list2cmdline(command)}"', directory)


def function_body(disassembly, name):
	match = re.search(r"^" + re.escape(name) + r":\r?$\n(.*?)(?=^[^\s][^\r\n]*:\r?$|\Z)",
		disassembly, re.MULTILINE | re.DOTALL)
	if not match:
		raise RuntimeError(f"Object disassembly has no function {name!r}")
	return match.group(1)


def zero_stores(body):
	return re.findall(r"\bmov\s+(?:byte|word|dword|qword) ptr \[[^\]]+\],0(?:h)?\s*$", body, re.MULTILINE)


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", type=Path)
	parser.add_argument("--build", type=Path, default=Path(".kiln/build/release"))
	options = parser.parse_args()
	build = (ROOT / options.build).resolve()
	compiler = (ROOT / options.compiler).resolve() if options.compiler else build / "target-636f6d70696c65722e657865/compiler.exe"
	output = build / "tests/complete-initialization"
	output.mkdir(parents=True, exist_ok=True)
	toolchain = Path("C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/Tools/VsDevCmd.bat")
	source = ROOT / "tests/compiler-new/backend/cases/complete-initialization.rt"
	anchor = output / "anchor.cpp"
	anchor.write_text("int complete_initialization_anchor = 0;\n")
	results = {}

	for optimization in (0, 2):
		object_path = output / f"transfer-o{optimization}.obj"
		compiled = checked([str(compiler), str(source), "--output", str(object_path),
			"--optimization", str(optimization)], output)
		(output / f"compile-o{optimization}.log").write_text(compiled.stdout + compiled.stderr)
		program = output / f"transfer-o{optimization}.exe"
		linked = native(["cl", "/nologo", "/EHsc", "/MD", str(anchor), str(object_path),
			"/Fe:" + str(program), "/link", "legacy_stdio_definitions.lib"], output, toolchain)
		(output / f"link-o{optimization}.log").write_text(linked.stdout + linked.stderr)
		checked([str(program)], output, 42)
		results[f"runtime-o{optimization}"] = "pass"
		print(f"complete-initialization runtime O{optimization}: PASS", flush=True)

		if not optimization:
			dumped = native(["dumpbin", "/disasm", str(object_path)], output, toolchain)
			(output / "transfer-o0.asm").write_text(dumped.stdout)
			for function in ("scalar_transfer", "move_transfer", "copy_transfer"):
				if zero_stores(function_body(dumped.stdout, function)):
					raise RuntimeError(f"{function}: destination is still cleared before complete construction")
				results[function + "-no-zero-store"] = "pass"
				print(f"{function} destination without redundant zero store (O0): PASS", flush=True)
			if not zero_stores(function_body(dumped.stdout, "Partial")):
				raise RuntimeError("User constructor lost its default member initialization")
			results["user-constructor-default-zeroing"] = "pass"

	(output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
	return 0


if __name__ == "__main__":
	try:
		sys.exit(main())
	except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
		print(error, file=sys.stderr)
		sys.exit(1)
