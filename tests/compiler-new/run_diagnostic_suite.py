"""Run compiler diagnostic suites and retain per-suite logs and a JSON summary."""

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
SUITES = (
	"run_diagnostics.py",
	"run_frontend_diagnostics.py",
	"run_declaration_diagnostics.py",
	"run_scope_diagnostics.py",
	"run_expression_diagnostics.py",
	"run_call_diagnostics.py",
	"run_initialization_diagnostics.py",
	"run_operator_diagnostics.py",
	"run_template_diagnostics.py",
	"run_inheritance_diagnostics.py",
	"run_switch_diagnostics.py",
	"run_lifetime_diagnostics.py",
	"run_backend_lifetime_diagnostics.py",
	"run_layout_diagnostics.py",
	"run_emission_diagnostics.py",
)


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--build", default=Path(".kiln/build/release"), type=Path)
	parser.add_argument("--compiler", type=Path)
	parser.add_argument("--object", type=Path)
	options = parser.parse_args()
	build = (ROOT / options.build).resolve()
	compiler = (ROOT / options.compiler).resolve() if options.compiler else build / "target-727574696c65632e657865/rutilec.exe"
	core = (ROOT / options.object).resolve() if options.object else build / "target-727574696c652d6c616e67756167652e6f626a/rutile-language.obj"
	configuration = build.name
	directory = ROOT / "build/diagnostics-tests" / configuration
	directory.mkdir(parents=True, exist_ok=True)
	summary = {
		"started_utc": datetime.now(timezone.utc).isoformat(),
		"build": str(build),
		"configuration": configuration,
		"compiler": str(compiler),
		"object": str(core),
		"results": [],
	}
	for suite in SUITES:
		command = [sys.executable, "-u", str(ROOT / "tests/compiler-new" / suite),
			"--compiler", str(compiler)]
		if suite == "run_emission_diagnostics.py":
			command.extend(["--build", str(build), "--object", str(core)])
		log = directory / (Path(suite).stem + ".log")
		started = time.monotonic()
		try:
			with log.open("w", encoding="utf-8") as stream:
				process = subprocess.run(command, cwd=ROOT, stdout=stream,
					stderr=subprocess.STDOUT, timeout=300)
			returncode = process.returncode
		except (OSError, subprocess.TimeoutExpired) as error:
			with log.open("a", encoding="utf-8") as stream:
				stream.write("\nSuite runner error: " + str(error) + "\n")
			returncode = None
		status = "passed" if returncode == 0 else "failed"
		summary["results"].append({
			"suite": suite,
			"status": status,
			"returncode": returncode,
			"elapsed_seconds": round(time.monotonic() - started, 3),
			"command": command,
			"log": str(log),
		})
		(directory / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
		print(f"{suite}: {status.upper()} ({log})", flush=True)

	failed = sum(result["status"] == "failed" for result in summary["results"])
	summary["finished_utc"] = datetime.now(timezone.utc).isoformat()
	summary["suites_passed"] = len(SUITES) - failed
	summary["suites_failed"] = failed
	(directory / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
	print(f"Diagnostic suites: {len(SUITES) - failed} passed, {failed} failed")
	return bool(failed)


if __name__ == "__main__":
	try:
		sys.exit(main())
	except (OSError, ValueError) as error:
		print(error, file=sys.stderr)
		sys.exit(1)
