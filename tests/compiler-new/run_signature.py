"""Check signature lookup and independent conditional qualification."""

import argparse
import sys

sys.dont_write_bytecode = True

from run import run_case as component_case


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--compiler", default="build/release/bin/rutile-selfhost.exe")
args = parser.parse_args()
failures = []


def run_case(name, sources, **options):
	try:
		component_case(name, sources, **options)
	except (OSError, RuntimeError, ValueError) as error:
		failures.append(name)
		print(error, file=sys.stderr)

for name in ["literal-argument", "temporary-receiver", "type-name-parameter", "auto-type-name",
	"template-arguments", "fixed-parameter", "independent-parameters", "grouped-pointer",
	"receiver-pointer", "named-argument"]:
	run_case("signature-" + name, ["tests/compiler/cases/constness-" + name + "/main.rt"],
		compiler=args.compiler)

run_case("signature-alias-shadow", ["tests/compiler-new/semantic/signature-shadow.rt"],
	compiler=args.compiler)
run_case("signature-pointer-discard", ["tests/compiler/cases/constness-pointer-discard/main.rt"],
	reject="expression cannot initialize the required type or reference",
	compiler=args.compiler)
run_case("signature-hidden-argument", ["tests/compiler/cases/constness-hidden-argument/main.rt"],
	reject="too many template arguments", compiler=args.compiler)
run_case("signature-missing-type", ["tests/compiler/cases/constness-auto-missing-type/main.rt"],
	reject="name does not denote a type", compiler=args.compiler)

run_case("signature-place-copy-ranking", ["tests/compiler/cases/implicit-same-type-overload-ranking/main.rt"],
	compiler=args.compiler)

run_case("signature-literal-reference", ["tests/compiler-new/semantic/literal-reference.rt"],
	compiler=args.compiler)
run_case("signature-literal-overload-ranking", ["tests/compiler-new/semantic/literal-overload-ranking.rt"],
	compiler=args.compiler)
for name in ["typed-place", "mutable"]:
	run_case("signature-literal-reference-" + name,
		["tests/compiler-new/semantic/literal-reference-" + name + ".rt"],
		reject="call has no unique applicable function", compiler=args.compiler)

run_case("signature-construction-ranking", ["tests/compiler-new/semantic/construction-ranking.rt"],
	compiler=args.compiler)

run_case("signature-string-view-construction-ranking",
	["std/unique.rt", "std/buffer.rt", "std/string_view.rt", "std/string.rt",
		"tests/compiler-new/semantic/string-view-construction-ranking.rt"], compiler=args.compiler)
run_case("signature-mixed-auto", ["tests/compiler-new/backend/cases/mixed-auto-qualification.rt"],
	compiler=args.compiler)
run_case("signature-mixed-auto-hidden-argument",
	["tests/compiler-new/backend/cases/mixed-auto-hidden-argument.rt"],
	reject="too many template arguments", compiler=args.compiler)

sys.exit(bool(failures))
