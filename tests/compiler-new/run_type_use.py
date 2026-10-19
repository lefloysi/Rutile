"""Verify callable type-use metadata through templates and owned pointer storage."""

import argparse
import sys

sys.dont_write_bytecode = True

from run import run_case


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", default=".kiln/build/release/target-636f6d70696c65722e657865/compiler.exe")
	args = parser.parse_args()
	cases = [("type-use-" + name, "tests/compiler-new/backend/cases/" + name + ".rt")
		for name in ["template-callable-labels", "template-nested-callable-labels",
			"template-forward-callable-labels", "allocation-callable-labels", "construction-callable-labels"]]
	cases += [("type-use-legacy-" + name, "tests/compiler/cases/" + name + "/main.rt")
		for name in ["call-interface-template-deduction", "call-interface-template-explicit",
			"call-interface-new", "type-equivalence-function-names"]]
	failures = []
	for name, source in cases:
		try:
			run_case(name, [source], compiler=args.compiler)
		except (OSError, RuntimeError, ValueError) as error:
			failures.append(name)
			print(error, file=sys.stderr)
	return bool(failures)


if __name__ == "__main__":
	sys.exit(main())
