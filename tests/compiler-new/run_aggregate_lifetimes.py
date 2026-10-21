"""Verify tuple and variant construction, copy, relocation, and active destruction."""

import argparse
from pathlib import Path
import sys

from run import run_case


ROOT = Path(__file__).resolve().parents[2]
PAYLOAD = """
i32 copies;
i32 destroyed;
struct Payload {
	i32 value;
	fn Payload(var Payload& this, i32 value) { @this.value := value; };
	fn Payload(var Payload& this, const Payload& other) {
		@copies = copies + 1;
		@this.value := other.value;
	};
	fn Payload(var Payload& this, Payload other) : [default];
	fn ~Payload(var Payload& this) { @destroyed = destroyed + 1; };
};
"""

CASES = [
	("variant-null-default-position", """
using First = null | i32;
using Last = i32 | null;
fn main() -> i32 {
	var first = First();
	var last = Last();
	if (!first.has<null>() || !last.has<null>()) { return 1; };
	if (first.get<null>() != nullptr || last.get<null>() != nullptr) { return 2; };
	return 42;
};
"""),
	("variant-only-null", """
using Empty = rt::variant<null>;
fn main() -> i32 {
	var empty = Empty();
	var explicit_null = Empty(nullptr);
	var copied = Empty(empty);
	var moved = Empty($explicit_null);
	if (!empty.has<null>() || !copied.has<null>() || !moved.has<null>()) { return 1; };
	return 42;
};
"""),
	("tuple-copy-move", """
using Pair = Payload & i64;
fn main() -> i32 {
	{
		var first = Pair(Payload(42), i64(0));
		var copied = Pair(first);
		var moved = Pair($first);
		if (copies != 1 || destroyed || copied.get<Payload>().value != 42 || moved.get<Payload>().value != 42) { return 1; };
	};
	if (destroyed != 2) { return 2; };
	return 42;
};
"""),
	("variant-copy-move", """
using Choice = Payload | i64;
fn main() -> i32 {
	{
		var source = Payload(42);
		var first = Choice(source);
		var copied = Choice(first);
		var moved = Choice($first);
		if (copies != 2 || destroyed || !copied.has<Payload>() || !moved.has<Payload>()) { return 1; };
		if (copied.get<Payload>().value != 42 || moved.get<Payload>().value != 42) { return 2; };
	};
	if (destroyed != 3) { return 3; };
	return 42;
};
"""),
	("variant-inactive-destruction", """
using Choice = Payload | null | i64;
fn main() -> i32 {
	{
		var empty = Choice();
		var explicit_null = Choice(nullptr);
		var scalar = Choice(i64(42));
		if (!empty.has<null>() || !explicit_null.has<null>() || empty.has<Payload>() || empty.has<i64>() || scalar.has<Payload>()) { return 1; };
	};
	if (copies || destroyed) { return 2; };
	return 42;
};
"""),
	("variant-nested-active-destruction", """
using Inner = Payload | i64;
using Outer = Inner | bool;
fn main() -> i32 {
	{
		var first = Inner(Payload(42));
		var outer = Outer($first);
		if (!outer.has<Inner>() || !outer.get<Inner>().has<Payload>() || outer.get<Inner>().get<Payload>().value != 42) { return 1; };
	};
	if (copies || destroyed != 1) { return 2; };
	return 42;
};
"""),
]

REJECTIONS = [
	("variant-no-default", """
using Choice = i32 | i64;
fn main() -> i32 { Choice value; return 42; };
""", r"(?i)(deleted|no applicable constructor|no.*constructor)"),
	("variant-null-not-alternative", """
using Choice = i32 | i64;
fn main() -> i32 { var value = Choice(nullptr); return 42; };
""", r"variant construction value type is not an alternative"),
]


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", type=Path, required=True)
	parser.add_argument("--case")
	options = parser.parse_args()
	for name, source in CASES:
		if options.case and options.case != name:
			continue
		directory = ROOT / "build/compiler-new/aggregate-lifetimes" / name
		directory.mkdir(parents=True, exist_ok=True)
		path = directory / "case.rt"
		path.write_text(PAYLOAD + source, encoding="utf-8")
		run_case("aggregate-lifetimes/" + name,
			[ROOT / "compiler/std/tuple.rt", ROOT / "compiler/std/variant.rt", path],
			compiler=options.compiler.resolve())
	for name, source, reject in REJECTIONS:
		if options.case and options.case != name:
			continue
		directory = ROOT / "build/compiler-new/aggregate-lifetimes" / name
		directory.mkdir(parents=True, exist_ok=True)
		path = directory / "case.rt"
		path.write_text(source, encoding="utf-8")
		run_case("aggregate-lifetimes/" + name,
			[ROOT / "compiler/std/tuple.rt", ROOT / "compiler/std/variant.rt", path],
			reject=reject, compiler=options.compiler.resolve())
	print("aggregate lifetimes: passed")
	return 0


if __name__ == "__main__":
	try:
		sys.exit(main())
	except (OSError, RuntimeError, ValueError) as error:
		print(error, file=sys.stderr)
		sys.exit(1)
