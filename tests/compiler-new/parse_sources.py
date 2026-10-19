"""Check the new parser against every current compiler and standard-library source."""
from pathlib import Path
import sys

sys.dont_write_bytecode = True
from run import ROOT, run_case

standard = ["compiler/std/unique.rt", "compiler/std/optional.rt", "compiler/std/buffer.rt", "compiler/std/string_view.rt",
	"compiler/std/string.rt", "compiler/std/hash.rt", "compiler/std/equal.rt", "compiler/std/unordered_map.rt"]
frontend = ["compiler/src/frontend/token.rt", "compiler/src/frontend/source.rt", "compiler/src/frontend/lexer.rt", "compiler/src/syntax/tree.rt", "compiler/src/frontend/parser.rt"]
driver = ["compiler/src/driver/file.rt", "compiler/src/driver/diagnostics.rt", "compiler/src/driver/source_reader.rt"]
paths = sorted([*ROOT.joinpath("compiler/std").rglob("*.rt"), *ROOT.joinpath("compiler/src").rglob("*.rt")])
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
