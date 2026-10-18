"""Check the new parser against every current compiler and standard-library source."""
from pathlib import Path
import sys

sys.dont_write_bytecode = True
from run import ROOT, run_case

standard = ["kiln/compiler/std/unique.rt", "kiln/compiler/std/optional.rt", "kiln/compiler/std/buffer.rt", "kiln/compiler/std/string_view.rt",
	"kiln/compiler/std/string.rt", "kiln/compiler/std/hash.rt", "kiln/compiler/std/equal.rt", "kiln/compiler/std/unordered_map.rt"]
frontend = ["kiln/compiler/src/frontend/token.rt", "kiln/compiler/src/frontend/source.rt", "kiln/compiler/src/frontend/lexer.rt", "kiln/compiler/src/syntax/tree.rt", "kiln/compiler/src/frontend/parser.rt"]
driver = ["kiln/compiler/src/driver/file.rt", "kiln/compiler/src/driver/diagnostics.rt", "kiln/compiler/src/driver/source_reader.rt"]
paths = sorted([*ROOT.joinpath("kiln/compiler/std").rglob("*.rt"), *ROOT.joinpath("kiln/compiler/src").rglob("*.rt")])
fixture = ROOT / "build/compiler-new/parser-own-sources/sources.rt"
fixture.parent.mkdir(parents=True, exist_ok=True)
lines = ["fn main() -> i32 {", "i32 result = 0;"]
for path in paths:
    text = path.as_posix().replace('"', '\\"')
    lines += [f'@result = parse_source_file("{text}\\0");', "if (result) { return result; };"]
lines += ["return 42;", "};"]
fixture.write_text("\n".join(lines))
run_case("parser-own-sources", [*standard, *frontend, *driver, "tests/compiler-new/frontend/parser_sources.rt", fixture.relative_to(ROOT).as_posix()])
print(f"Parsed {len(paths)} source files.")
