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
	("variant-copy-move-assignment", """
using Choice = Payload | null | i64;
fn main() -> i32 {
	{
		var first = Choice(Payload(42));
		var second = Choice(Payload(1));
		@second := first;
		if (copies || destroyed || first.get<Payload>().value != 42 || second.get<Payload>().value != 42) { return 1; };
		@second = Choice(i64(9));
		if (!second.has<i64>() || destroyed != 1) { return 2; };
		@second = Choice(Payload(7));
		if (!second.has<Payload>() || second.get<Payload>().value != 7) { return 3; };
		@first := first;
		if (copies || destroyed != 1 || first.get<Payload>().value != 42) { return 4; };
	};
	if (destroyed != 3) { return 5; };
	return 42;
};
"""),
	("variant-null-default-position", """
using First = null | i32;
using Last = i32 | null;
fn main() -> i32 {
	var first = First();
	var last = Last();
	if (!first.has<null>() || !last.has<i32>()) { return 1; };
	if (first.get<null>() != nullptr || last.get<i32>() != 0) { return 2; };
	return 42;
};
"""),

	("variant-monostate-first", """
using Choice = rt::monostate | Payload;
fn main() -> i32 {
	var first = Choice();
	var moved = Choice($first);
	if (!moved.has<rt::monostate>()) { return 1; };
	rt::monostate& empty = moved.get<rt::monostate>();
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
		var empty = Choice(nullptr);
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

	("variant-first-record-default", """
struct First {
	i32 value;
	fn First(var First& this) noexcept { @this.value := 42; };
};
struct PotentiallyThrowingFirst {
	fn PotentiallyThrowingFirst(var PotentiallyThrowingFirst& this) {};
};
using Choice = First | null | i64;
using ThrowingChoice = PotentiallyThrowingFirst | null;
fn main() -> i32 {
	var value = Choice();
	if (!value.has<First>() || !noexcept(Choice()) || noexcept(ThrowingChoice())) { return 1; };
	return value.get<First>().value;
};
"""),
	("variant-payload-copy-assignment", """
i32 assignments;
struct Assigned {
	i32 value;
	fn Assigned(var Assigned& this, i32 value) { @this.value := value; };
	fn Assigned(var Assigned& this, Assigned& other) : [default];
	fn Assigned(var Assigned& this, Assigned other) : [default];
	fn operator:=(var Assigned& this, Assigned& other) -> var Assigned& {
		@assignments = assignments + 1;
		@this.value := other.value;
		return @this;
	};
};
using Choice = Assigned | i64;
fn main() -> i32 {
	var first = Choice(Assigned(42));
	var second = Choice(Assigned(1));
	@second := first;
	@first := first;
	if (assignments != 2 || first.get<Assigned>().value != 42) { return 1; };
	return second.get<Assigned>().value;
};
"""),
	("variant-throwing-copy-preserves-destination", """
struct ThrowingCopy {
	i32 value;
	fn ThrowingCopy(var ThrowingCopy& this, i32 value) { @this.value := value; };
	fn ThrowingCopy(var ThrowingCopy& this, ThrowingCopy& other) {
		var error = i32(42);
		throw $error;
	};
	fn ThrowingCopy(var ThrowingCopy& this, ThrowingCopy other) : [default];
};
using Choice = ThrowingCopy | i64;
fn main() -> i32 {
	var source = Choice(ThrowingCopy(7));
	var target = Choice(i64(42));
	try { @target := source; }
	catch (i32 error) {
		if (!source.has<ThrowingCopy>() || !target.has<i64>()) { return 1; };
		return i32(target.get<i64>());
	};
	return 2;
};
"""),

]

REJECTIONS = [
	("variant-no-default", """
struct NoDefault {
	fn NoDefault(var NoDefault& this, i32 value) {};
};
using Choice = NoDefault | rt::monostate | null | i64;
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
