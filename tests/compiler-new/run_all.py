"""Run the new compiler's component checks through one native bootstrap harness."""

import sys

sys.dont_write_bytecode = True

from run import run_case


standard = ["std/unique.rt", "std/buffer.rt",
	"std/string_view.rt", "std/string.rt"]
frontend = [*standard, "compiler/src/frontend/token.rt", "compiler/src/frontend/source.rt",
	"compiler/src/frontend/lexer.rt"]

for name in ["buffer", "buffer-lifetime", "buffer-alias", "buffer-owners", "unique", "text"]:
	run_case("std-" + name, [*standard, "tests/compiler-new/std/" + name + ".rt"])
for name, diagnostic in [("buffer-const", "pointer conversion cannot discard const qualification"),
	("buffer-copy", "deleted"), ("unique-copy", "deleted")]:
	run_case("std-" + name, [*standard, "tests/compiler-new/std/" + name + ".rt"], reject=diagnostic)

run_case("frontend-lexer", [*frontend, "tests/compiler-new/frontend/lexer.rt"],
	entry="rutile_frontend_tests", expected=0)
run_case("frontend-parser", [*frontend, "compiler/src/syntax/tree.rt",
	"compiler/src/frontend/parser.rt", "tests/compiler-new/frontend/parser.rt"], entry="parser_tests")
run_case("semantic-types", [*standard, "compiler/src/semantic/type.rt",
	"compiler/src/semantic/type_store.rt", "tests/compiler-new/semantic/types.rt"])
run_case("source-reader", [*frontend, "compiler/src/driver/file.rt",
	"compiler/src/driver/diagnostics.rt", "compiler/src/driver/source_reader.rt",
	"tests/compiler-new/driver/source_reader.rt"])
