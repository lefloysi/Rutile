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
	"enum-value-copy", "out-of-line-access", "template-extension", "function-pointer-callees",
	"array-empty-scalars", "sizeof-operands", "typed-pointer-construction", "numeric-context-values",
	"loop-result-reachability", "contextual-reference", "constant-contextual",
	"generated-copy-assignment", "generated-copy-assignment-conversions"]:
	check(name, sources + name + ".rt")

check("lifetime-access-relocation", sources + "lifetime-access-relocation.rt",
	reject="object has been relocated")
check("lifetime-destroyed-use", sources + "lifetime-destroyed-use.rt",
	reject="object has been destroyed")
check("lifetime-reference-relocation", sources + "lifetime-reference-relocation.rt",
	reject="object has been relocated")
check("contextual-reference-escape", sources + "contextual-reference-escape.rt",
	reject="sequence result refers to local storage")
check("contextual-ambiguous", sources + "contextual-ambiguous.rt", reject="function selection is ambiguous")
check("contextual-unlabeled-empty", sources + "contextual-unlabeled-empty.rt",
	reject="argument block requires a named parameter")
check("call-interface-unlabeled", "tests/compiler/cases/call-interface-unlabeled/main.rt",
	reject="argument block requires a named parameter")
for name in ["loop-result-break", "loop-result-false"]:
	check(name, sources + name + ".rt", reject="non-void function reaches the end without a result")
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
	reject="not accessible from this scope")
check("template-private-function", sources + "template-private-function.rt",
	reject="not accessible from this scope")
for name in ["sizeof-incomplete", "sizeof-void"]:
	check(name, sources + name + ".rt", reject="sizeof requires a complete object storage type")

for name in ["owned-lifetimes", "scope-exits", "allocation-lifetimes", "return-lifetimes",
	"generated-move", "complete-initialization", "reconstruction", "switch", "array-lifetimes", "array-invalid-size",
	"union-layout", "union-lifetimes", "member-initialization", "inheritance-casts",
	"inheritance-lifetimes", "virtual-dispatch", "virtual-layout", "external-enum", "aggregate-copy",
	"inactive-array-storage", "lifetime-conditional-unused", "reference-call-temporary",
	"contextual-argument-lifetimes"]:
	check(name, "tests/compiler-new/backend/cases/" + name + ".rt")
check("lifetime-conditional-use", "tests/compiler-new/backend/cases/lifetime-conditional-use.rt",
	reject="object lifetime differs across reachable control-flow paths")

for name in ["automatic-move", "generated-copy-members", "generated-copy-assignment",
	"explicit-copy-preserved", "user-move-preserved", "primitive-constants", "index-extension"]:
	check("backend-" + name, "tests/compiler-new/backend/cases/" + name + ".rt")
for name in ["deleted-copy-preserved", "deleted-move-preserved"]:
	check("backend-" + name, "tests/compiler-new/backend/cases/" + name + ".rt", reject="deleted")
for name in ["static-array-overflow", "nested-array-overflow"]:
	check("backend-" + name, "tests/compiler-new/backend/cases/" + name + ".rt",
		reject="array storage size exceeds target address space")
check("generated-copy-assignment-deleted", sources + "generated-copy-assignment-deleted.rt",
	reject="deleted")
check("generated-place-move-reject", sources + "generated-place-move-reject.rt",
	reject="no applicable operator '='")
check("generated-string-copy", sources + "generated-string-copy.rt",
	dependencies=["kiln/compiler/std/unique.rt", "kiln/compiler/std/buffer.rt",
		"kiln/compiler/std/string_view.rt", "kiln/compiler/std/string.rt"])
for name in ["null-constant-missing", "void-constant-missing"]:
	check("backend-" + name, "tests/compiler-new/backend/cases/" + name + ".rt",
		reject="unknown identifier")

for name in ["external-array-return", "external-record-parameter"]:
	check(name, "tests/compiler-new/backend/cases/" + name + ".rt", reject="unsupported C ABI")
check("variadic-aggregate", "tests/compiler-new/backend/cases/variadic-aggregate.rt",
	reject="variadic")

for name in ["optional", "optional-ref", "optional-lifetime"]:
	check("native-" + name, "tests/compiler-new/std/" + name + ".rt",
		dependencies=["kiln/compiler/std/optional.rt"])
check("native-optional-copy", "tests/compiler-new/std/optional-copy.rt", reject="deleted",
	dependencies=["kiln/compiler/std/optional.rt"])
check("native-unique-policy", "tests/compiler-new/std/unique-policy.rt",
	dependencies=["kiln/compiler/std/unique.rt"])
standard = ["kiln/compiler/std/unique.rt", "kiln/compiler/std/optional.rt", "kiln/compiler/std/buffer.rt", "kiln/compiler/std/string_view.rt",
	"kiln/compiler/std/string.rt", "kiln/compiler/std/hash.rt", "kiln/compiler/std/equal.rt", "kiln/compiler/std/unordered_map.rt"]
for name in ["buffer", "buffer-copy", "buffer-lifetime", "buffer-clear-order", "buffer-alias", "buffer-owners", "unique", "text",
	"hash", "unordered-map", "unordered-map-collisions", "unordered-map-default-hash",
	"unordered-map-function-hash"]:
	check("native-" + name, "tests/compiler-new/std/" + name + ".rt", dependencies=standard)
check("native-primitive-registry", sources + "primitive-registry.rt",
	dependencies=[*standard, "kiln/compiler/src/semantic/type.rt", "kiln/compiler/src/semantic/type_store.rt",
		"kiln/compiler/src/semantic/primitives.rt"])
check("function-parameter-labels", "tests/compiler/cases/type-equivalence-function-names/main.rt")
check("pointer-difference-type", "tests/compiler/cases/ptrdiff/main.rt")
check("implicit-builtin-copies", "tests/compiler/cases/implicit-same-type-builtins/main.rt")
check("implicit-copy-mutable-receiver", "tests/compiler/cases/implicit-same-type-mutable-receiver/main.rt",
	reject="self conversion requires a const reference receiver")

check("template-unused-deferred", "tests/compiler/cases/template-unused-deferred/main.rt")
check("template-unused-nonintegral", "tests/compiler/cases/template-unused-nonintegral/main.rt",
	reject="non-type template parameter requires an integral type")
check("template-unused-duplicate", "tests/compiler/cases/template-unused-duplicate/main.rt",
	reject="duplicate declaration in template member scope")
check("template-unused-receiver", sources + "template-validation-constructor-receiver.rt",
	reject="instance function requires an explicit 'this' reference parameter")

check("template-validation-ordinary-operator", sources + "template-validation-ordinary-operator.rt")
check("template-validation-value-shadow", sources + "template-validation-value-shadow.rt",
	reject="identifier does not name a type entity")
check("template-validation-sibling-scope", sources + "template-validation-sibling-scope.rt",
	reject="identifier does not name a type entity")
check("template-validation-scope-restoration", sources + "template-validation-scope-restoration.rt")
check("template-validation-virtual-receiver", sources + "template-validation-virtual-receiver.rt",
	reject="virtual operation 'function' requires an explicit class receiver of reference type named 'this'")

check("union-defaulted-lifetimes", "tests/compiler-new/backend/cases/union-defaulted-lifetimes.rt")
check("union-missing-destructor", "tests/compiler-new/backend/cases/union-missing-destructor.rt",
	reject="union 'Missing' requires an explicit destructor because member 'Missing::value'")

check("native-unordered-map-function-default-hash", "tests/compiler-new/std/unordered-map-function-default-hash.rt",
	reject="no applicable function", dependencies=standard)

if failures:
	sys.exit(1)
