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


standard = ["std/unique.rt", "std/optional.rt", "std/buffer.rt",
	"std/string_view.rt", "std/string.rt", "std/hash.rt", "std/equal.rt", "std/unordered_map.rt"]
frontend = [*standard, "compiler/src/frontend/token.rt", "compiler/src/frontend/source.rt",
	"compiler/src/frontend/lexer.rt"]

for name in ["buffer", "buffer-lifetime", "buffer-copy", "buffer-alias", "buffer-owners", "unique", "text",
	"optional", "optional-ref", "optional-lifetime", "unique-policy", "hash", "unordered-map",
	"unordered-map-collisions"]:
	run_case("std-" + name, [*standard, "tests/compiler-new/std/" + name + ".rt"])
for name, diagnostic in [("buffer-const", "expression cannot initialize the required type or reference"),
	("unique-copy", "deleted")]:
	run_case("std-" + name, [*standard, "tests/compiler-new/std/" + name + ".rt"], reject=diagnostic)

run_case("std-optional-ref-const", [*standard, "tests/compiler-new/std/optional-ref-const.rt"],
	reject="assignment requires explicit mutable access to a place")
run_case("std-optional-const", [*standard, "tests/compiler-new/std/optional-const.rt"],
	reject="assignment requires explicit mutable access to a place")
run_case("std-optional-copy", [*standard, "tests/compiler-new/std/optional-copy.rt"], reject="deleted")

run_case("frontend-lexer", [*frontend, "tests/compiler-new/frontend/lexer.rt"],
	entry="rutile_frontend_tests", expected=0)
run_case("frontend-parser", [*frontend, "compiler/src/syntax/tree.rt",
	"compiler/src/frontend/parser.rt", "tests/compiler-new/frontend/parser.rt"], entry="parser_tests")
run_case("frontend-typed-callee", [*frontend, "compiler/src/syntax/tree.rt",
	"compiler/src/frontend/parser.rt", "tests/compiler-new/frontend/parser.rt",
	"tests/compiler-new/frontend/typed-callee.rt"], entry="typed_callee_tests")
run_case("semantic-types", [*standard, "compiler/src/semantic/type.rt",
	"compiler/src/semantic/type_store.rt", "compiler/src/semantic/primitives.rt",
	"tests/compiler-new/semantic/types.rt"])
run_case("primitive-registry", [*standard, "compiler/src/semantic/type.rt",
	"compiler/src/semantic/type_store.rt", "compiler/src/semantic/primitives.rt",
	"tests/compiler-new/semantic/primitive-registry.rt"])
run_case("constant-queries", [*frontend, "compiler/src/syntax/tree.rt",
	"compiler/src/semantic/type.rt", "compiler/src/semantic/type_store.rt",
	"compiler/src/semantic/primitives.rt",
	"compiler/src/semantic/type_use.rt",
	"compiler/src/semantic/entity.rt", "compiler/src/semantic/scope.rt",
	"compiler/src/semantic/expression.rt", "compiler/src/semantic/type_queries.rt",
	"compiler/src/semantic/copy_assignment.rt",
	"compiler/src/semantic/constant_queries.rt", "compiler/src/semantic/real_queries.rt",
	"tests/compiler-new/semantic/constant-queries.rt"])
run_case("source-reader", [*frontend, "compiler/src/driver/file.rt",
	"compiler/src/driver/diagnostics.rt", "compiler/src/driver/source_reader.rt",
	"tests/compiler-new/driver/source_reader.rt"])

semantic = ["compiler/src/" + path for path in (ROOT / "compiler/sources.list").read_text().splitlines()
	if path.startswith("semantic/")]
model = [*frontend, "compiler/src/syntax/tree.rt", "compiler/src/frontend/parser.rt",
	"compiler/src/driver/file.rt", "compiler/src/driver/diagnostics.rt", *semantic,
	"tests/compiler-new/semantic/templates.rt"]
run_case("primitive-scopes", [*model, "tests/compiler-new/semantic/primitive-scopes.rt"],
	entry="primitive_scope_tests")
run_case("semantic-templates", model, entry="template_tests")
run_case("semantic-type-use", [*model, "tests/compiler-new/semantic/type-use.rt"],
	entry="type_use_tests")
run_case("semantic-local-alias", [*model, "tests/compiler-new/semantic/local-alias.rt"],
	entry="local_alias_tests")
run_case("constant-evaluator", [*model, "tests/compiler-new/semantic/constant-evaluator.rt"],
	entry="constant_tests")
run_case("constant-reference-bindings", [*model,
	"tests/compiler-new/semantic/constant-reference-bindings.rt"], entry="reference_tests")
run_case("numeric-context", [*model, "tests/compiler-new/semantic/numeric-context.rt"],
	entry="numeric_context_tests")
