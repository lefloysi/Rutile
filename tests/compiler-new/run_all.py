"""Run the new compiler's component checks through one native bootstrap harness."""

import sys

sys.dont_write_bytecode = True

from run import ROOT, run_case


standard = ["std/unique.rt", "std/optional.rt", "std/buffer.rt",
	"std/string_view.rt", "std/string.rt"]
frontend = [*standard, "compiler/src/frontend/token.rt", "compiler/src/frontend/source.rt",
	"compiler/src/frontend/lexer.rt"]

for name in ["buffer", "buffer-lifetime", "buffer-alias", "buffer-owners", "unique", "text",
	"optional", "optional-ref", "optional-lifetime", "unique-policy"]:
	run_case("std-" + name, [*standard, "tests/compiler-new/std/" + name + ".rt"])
for name, diagnostic in [("buffer-const", "pointer conversion cannot discard const qualification"),
	("buffer-copy", "deleted"), ("unique-copy", "deleted")]:
	run_case("std-" + name, [*standard, "tests/compiler-new/std/" + name + ".rt"], reject=diagnostic)

run_case("std-optional-ref-const", [*standard, "tests/compiler-new/std/optional-ref-const.rt"],
	reject="immutable object cannot provide mutable access")
run_case("std-optional-const", [*standard, "tests/compiler-new/std/optional-const.rt"],
	reject="immutable object cannot provide mutable access")
run_case("std-optional-copy", [*standard, "tests/compiler-new/std/optional-copy.rt"], reject="deleted")

run_case("frontend-lexer", [*frontend, "tests/compiler-new/frontend/lexer.rt"],
	entry="rutile_frontend_tests", expected=0)
run_case("frontend-parser", [*frontend, "compiler/src/syntax/tree.rt",
	"compiler/src/frontend/parser.rt", "tests/compiler-new/frontend/parser.rt"], entry="parser_tests")
run_case("semantic-types", [*standard, "compiler/src/semantic/type.rt",
	"compiler/src/semantic/type_store.rt", "tests/compiler-new/semantic/types.rt"])
run_case("constant-queries", [*frontend, "compiler/src/syntax/tree.rt",
	"compiler/src/semantic/type.rt", "compiler/src/semantic/type_store.rt",
	"compiler/src/semantic/entity.rt", "compiler/src/semantic/scope.rt",
	"compiler/src/semantic/expression.rt", "compiler/src/semantic/type_queries.rt",
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
run_case("semantic-templates", model, entry="template_tests")
run_case("constant-evaluator", [*model, "tests/compiler-new/semantic/constant-evaluator.rt"],
	entry="constant_tests")
