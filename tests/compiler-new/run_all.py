"""Run the new compiler's component checks through one native bootstrap harness."""

import argparse
import sys

sys.dont_write_bytecode = True

from run import ROOT, run_case as component_case


arguments = argparse.ArgumentParser(description=__doc__)
arguments.add_argument("--compiler", default="build/release/bin/rutile-selfhost.exe")
configuration = arguments.parse_args()


def run_case(name, sources, **options):
	return component_case(name, sources, compiler=configuration.compiler, **options)


standard = ["kiln/compiler/std/unique.rt", "kiln/compiler/std/optional.rt", "kiln/compiler/std/buffer.rt",
	"kiln/compiler/std/string_view.rt", "kiln/compiler/std/string.rt", "kiln/compiler/std/hash.rt", "kiln/compiler/std/equal.rt", "kiln/compiler/std/unordered_map.rt"]
frontend = [*standard, "kiln/compiler/src/frontend/token.rt", "kiln/compiler/src/frontend/source.rt",
	"kiln/compiler/src/frontend/lexer.rt"]

for name in ["buffer", "buffer-lifetime", "buffer-clear-order", "buffer-copy", "buffer-alias", "buffer-owners", "unique", "text", "string-sso",
	"optional", "optional-ref", "optional-lifetime", "unique-policy", "hash", "unordered-map",
	"unordered-map-collisions", "unordered-map-default-hash", "unordered-map-function-hash"]:
	run_case("std-" + name, [*standard, "tests/compiler-new/std/" + name + ".rt"])
for name, diagnostic in [("buffer-const", "cannot initialize"),
	("unique-copy", "deleted"), ("unordered-map-function-default-hash", "no applicable function")]:
	run_case("std-" + name, [*standard, "tests/compiler-new/std/" + name + ".rt"], reject=diagnostic)

run_case("std-optional-ref-const", [*standard, "tests/compiler-new/std/optional-ref-const.rt"],
	reject="assignment requires explicit mutable access to a place")
run_case("std-optional-const", [*standard, "tests/compiler-new/std/optional-const.rt"],
	reject="assignment requires explicit mutable access to a place")
run_case("std-optional-copy", [*standard, "tests/compiler-new/std/optional-copy.rt"], reject="deleted")

run_case("frontend-lexer", [*frontend, "tests/compiler-new/frontend/lexer.rt"],
	entry="rutile_frontend_tests", expected=0)
run_case("frontend-parser", [*frontend, "kiln/compiler/src/syntax/tree.rt",
	"kiln/compiler/src/frontend/parser.rt", "tests/compiler-new/frontend/parser.rt"], entry="parser_tests")
run_case("frontend-editor-recovery", [*frontend, "kiln/compiler/src/syntax/tree.rt",
	"kiln/compiler/src/frontend/parser.rt", "tests/compiler-new/frontend/editor-recovery.rt"], entry="editor_recovery_tests")
run_case("frontend-typed-callee", [*frontend, "kiln/compiler/src/syntax/tree.rt",
	"kiln/compiler/src/frontend/parser.rt", "tests/compiler-new/frontend/parser.rt",
	"tests/compiler-new/frontend/typed-callee.rt"], entry="typed_callee_tests")
run_case("semantic-types", [*standard, "kiln/compiler/src/semantic/type.rt",
	"kiln/compiler/src/semantic/type_store.rt", "kiln/compiler/src/semantic/primitives.rt",
	"tests/compiler-new/semantic/types.rt"])
run_case("primitive-registry", [*standard, "kiln/compiler/src/semantic/type.rt",
	"kiln/compiler/src/semantic/type_store.rt", "kiln/compiler/src/semantic/primitives.rt",
	"tests/compiler-new/semantic/primitive-registry.rt"])
run_case("constant-queries", [*frontend, "kiln/compiler/src/syntax/tree.rt",
	"kiln/compiler/src/semantic/type.rt", "kiln/compiler/src/semantic/type_store.rt",
	"kiln/compiler/src/semantic/primitives.rt",
	"kiln/compiler/src/semantic/type_use.rt",
	"kiln/compiler/src/semantic/entity.rt", "kiln/compiler/src/semantic/scope.rt",
	"kiln/compiler/src/semantic/expression.rt", "kiln/compiler/src/semantic/type_queries.rt",
	"kiln/compiler/src/semantic/copy_assignment.rt",
	"kiln/compiler/src/semantic/constant_queries.rt", "kiln/compiler/src/semantic/real_queries.rt",
	"tests/compiler-new/semantic/constant-queries.rt"])
run_case("source-reader", [*frontend, "kiln/compiler/src/driver/file.rt",
	"kiln/compiler/src/driver/diagnostics.rt", "kiln/compiler/src/driver/source_reader.rt",
	"tests/compiler-new/driver/source_reader.rt"])

semantic = ["kiln/compiler/src/" + path for path in (ROOT / "kiln/compiler/sources.list").read_text().splitlines()
	if path.startswith("semantic/")]
model = [*frontend, "kiln/compiler/src/syntax/tree.rt", "kiln/compiler/src/frontend/parser.rt",
	"kiln/compiler/src/driver/file.rt", "kiln/compiler/src/driver/diagnostics.rt", *semantic,
	"tests/compiler-new/semantic/templates.rt"]
run_case("primitive-scopes", [*model, "tests/compiler-new/semantic/primitive-scopes.rt"],
	entry="primitive_scope_tests")
run_case("semantic-templates", model, entry="template_tests")
run_case("semantic-template-reference-inputs", [*model,
	"tests/compiler-new/semantic/template-reference-inputs.rt"], entry="template_reference_input_tests")
run_case("semantic-template-validation", [*model, "tests/compiler-new/semantic/template-validation.rt"],
	entry="template_validation_tests")
run_case("semantic-type-use", [*model, "tests/compiler-new/semantic/type-use.rt"],
	entry="type_use_tests")
run_case("semantic-callable-type-use", [*model, "tests/compiler-new/semantic/callable-type-use.rt"],
	entry="callable_type_use_tests")
run_case("semantic-mixed-qualification", [*model, "tests/compiler-new/semantic/mixed-qualification.rt"],
	entry="mixed_qualification_tests")
run_case("semantic-local-alias", [*model, "tests/compiler-new/semantic/local-alias.rt"],
	entry="local_alias_tests")
run_case("constant-evaluator", [*model, "tests/compiler-new/semantic/constant-evaluator.rt"],
	entry="constant_tests")
run_case("constant-reference-bindings", [*model,
	"tests/compiler-new/semantic/constant-reference-bindings.rt"], entry="reference_tests")
run_case("numeric-context", [*model, "tests/compiler-new/semantic/numeric-context.rt"],
	entry="numeric_context_tests")
