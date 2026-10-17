"""Check language behavior through the replacement compiler's native output."""

import sys

sys.dont_write_bytecode = True

from run import run_case


compiler = "build/release/bin/rutile-selfhost.exe"
sources = "tests/compiler-new/semantic/"
failures = []


def check(name, source, reject=None, dependencies=()):
	try:
		run_case(name, [*dependencies, source], reject=reject, compiler=compiler)
	except (OSError, RuntimeError, ValueError) as error:
		failures.append(name)
		print(error, file=sys.stderr)


for name in ["lifetime-constant-path", "lifetime-pointer-relocation", "lifetime-reconstruction",
	"template-class", "template-function", "template-namespace", "template-member", "template-auto",
	"template-value", "template-recursive", "template-default", "template-qualified",
	"ordinary-initialization", "ordinary-operators", "switch-loop-control", "variadic-promotions",
	"variadic-call", "literal-conversions", "constant-calls", "constant-template-call",
	"inferred-assignment", "unused-inferred-assignment", "pointer-void", "template-function-address",
	"template-policy", "explicit-copy", "out-of-line-assignment", "inferred-copy-assignment",
	"enum-value-copy", "out-of-line-access", "template-extension", "function-pointer-callees"]:
	check(name, sources + name + ".rt")

check("lifetime-access-relocation", sources + "lifetime-access-relocation.rt",
	reject="relocated or discarded")
check("lifetime-destroyed-use", sources + "lifetime-destroyed-use.rt",
	reject="object has been destroyed")
check("switch-duplicate", sources + "switch-duplicate.rt", reject="duplicate switch case value")
check("constant-limit", sources + "constant-limit.rt", reject="integer constant")
check("constant-mutable-global", sources + "constant-mutable-global.rt", reject="integer constant")
check("reference-field-temporary", sources + "reference-field-temporary.rt",
	reject="reference member initializer requires a live place")
check("pointer-void-const", sources + "pointer-void-const.rt",
	reject="expression cannot initialize the required type or reference")
check("pointer-integer-comparison", sources + "pointer-integer-comparison.rt",
	reject="pointer comparison requires compatible pointer types")
check("using-types", "tests/compiler/cases/using-types/main.rt")
check("using-cycle", "tests/compiler/cases/using-cycle/main.rt", reject="cyclic entity definition")
check("explicit-copy-deleted-member", sources + "explicit-copy-deleted-member.rt", reject="deleted")
check("inferred-copy-assignment-deleted", sources + "inferred-copy-assignment-deleted.rt",
	reject="deleted")
check("out-of-line-private", sources + "out-of-line-private.rt", reject="not accessible")
check("template-extension-private", sources + "template-extension-private.rt",
	reject="private or protected function")
check("template-private-function", sources + "template-private-function.rt",
	reject="private or protected function")

for name in ["owned-lifetimes", "scope-exits", "allocation-lifetimes", "return-lifetimes",
	"generated-move", "reconstruction", "switch", "array-lifetimes", "array-invalid-size",
	"union-layout", "union-lifetimes", "member-initialization", "inheritance-casts",
	"inheritance-lifetimes", "virtual-dispatch", "virtual-layout", "external-enum", "aggregate-copy",
	"inactive-array-storage"]:
	check(name, "tests/compiler-new/backend/cases/" + name + ".rt")

for name in ["external-array-return", "external-record-parameter"]:
	check(name, "tests/compiler-new/backend/cases/" + name + ".rt", reject="unsupported C ABI")
check("variadic-aggregate", "tests/compiler-new/backend/cases/variadic-aggregate.rt",
	reject="variadic")

for name in ["optional", "optional-ref", "optional-lifetime"]:
	check("native-" + name, "tests/compiler-new/std/" + name + ".rt",
		dependencies=["std/optional.rt"])
check("native-optional-copy", "tests/compiler-new/std/optional-copy.rt", reject="deleted",
	dependencies=["std/optional.rt"])
check("native-unique-policy", "tests/compiler-new/std/unique-policy.rt",
	dependencies=["std/unique.rt"])

if failures:
	sys.exit(1)
