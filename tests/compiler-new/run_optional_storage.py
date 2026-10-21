"""Verify optional's inline storage while preserving its public behavior."""

import argparse
from pathlib import Path
import sys

from run import ROOT, run_case


DEPENDENCIES = ["compiler/std/optional.rt"]
FIXTURES = [
	("optional", None),
	("optional-ref", None),
	("optional-lifetime", None),
	("optional-const", "assignment requires explicit mutable access to a place"),
	("optional-ref-const", "assignment requires explicit mutable access to a place"),
	("optional-copy", "deleted"),
]

CASES = [
	("record-copy-assignment", """
i32 copies;
i32 destructions;
struct Payload {
	i32 number;
	fn Payload(var Payload& this, i32 number) { @this.number := number; };
	fn Payload(var Payload& this, const Payload& other) {
		@copies = copies + 1;
		@this.number := other.number;
	};
	fn Payload(var Payload& this, Payload other) : [default];
	fn ~Payload(var Payload& this) { @destructions = destructions + 1; };
};
fn main() -> i32 {
	{
		var first = rt::optional<Payload>(Payload(42));
		var second = rt::optional<Payload>(Payload(1));
		@second := first;
		if (copies != 1 || destructions != 1 || first.value().number != 42 || second.value().number != 42) { return 1; };
	};
	if (destructions != 3) { return 2; };
	return 42;
};
"""),
	("self-copy-assignment", """
fn main() -> i32 {
	var empty = rt::optional<i32>();
	@empty := empty;
	if (empty.has_value() || empty.get()) { return 1; };
	var value = rt::optional<i32>(42);
	@value := value;
	return value.value();
};
"""),
	("const-pointer-access", """
fn main() -> i32 {
	const rt::optional<i32> value = rt::optional<i32>(42);
	const i32* pointer = value.get();
	return *pointer;
};
"""),
	("empty-copy-move-and-replacement", """
fn main() -> i32 {
	var empty = rt::optional<i32>();
	var copied_empty = rt::optional<i32>(empty);
	var moved_empty = rt::optional<i32>($empty);
	if (copied_empty.has_value() || copied_empty.get() || moved_empty.has_value() || moved_empty.get()) { return 1; };
	@copied_empty.emplace(10);
	@copied_empty.emplace(42);
	var copied = rt::optional<i32>(copied_empty);
	if (copied.value() != 42 || copied_empty.value() != 42) { return 2; };
	@copied_empty.reset();
	@copied_empty.reset();
	if (copied_empty.has_value() || copied_empty.get()) { return 3; };
	var moved = rt::optional<i32>($copied);
	if (!moved.has_value() || moved.value() != 42) { return 4; };
	@copied_empty = rt::optional<i32>($moved);
	var i32* pointer = (@copied_empty).get();
	@*pointer = 42;
	return copied_empty.value();
};
"""),
	("copyable-record-lifetimes", """
i32 constructions = 0;
i32 copies = 0;
i32 destructions = 0;
struct Payload {
	fn Payload(var Payload& this, i32 number) {
		@this.number := number;
		@constructions = constructions + 1;
	};
	fn Payload(var Payload& this, Payload& other) {
		@this.number := other.number;
		@copies = copies + 1;
	};
	fn Payload(var Payload& this, Payload other) : [default];
	fn ~Payload(var Payload& this) { @destructions = destructions + 1; };
	i32 number;
};
fn main() -> i32 {
	{
		var first = rt::optional<Payload>(Payload(42));
		var second = rt::optional<Payload>(first);
		if (constructions != 1 || copies != 1 || destructions || second.value().number != 42) { return 1; };
		@first.reset();
		if (destructions != 1 || first.has_value() || second.value().number != 42) { return 2; };
		var moved = rt::optional<Payload>($second);
		if (destructions != 1 || moved.value().number != 42) { return 3; };
		var Payload& replacement = @moved.emplace(Payload(21));
		@replacement.number = 42;
		if (constructions != 2 || copies != 1 || destructions != 2 || moved.value().number != 42) { return 5; };
	};
	if (constructions != 2 || copies != 1 || destructions != 3) { return 4; };
	return 42;
};
"""),
]


REJECTIONS = [
	("const-pointer-escape", """
fn main() -> i32 {
	const rt::optional<i32> value = rt::optional<i32>(42);
	var i32* pointer = value.get();
	return 42;
};
""", r"cannot initialize 'var i32\*'"),
]


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", type=Path)
	options = parser.parse_args()
	compiler = options.compiler.resolve() if options.compiler else None
	for name, reject in FIXTURES:
		run_case("optional-storage/" + name, [*DEPENDENCIES,
			"tests/compiler-new/std/" + name + ".rt"], reject=reject, compiler=compiler)
	for name, source in CASES:
		directory = ROOT / "build/compiler-new/optional-storage" / name
		directory.mkdir(parents=True, exist_ok=True)
		path = directory / "case.rt"
		path.write_text(source.strip() + "\n", encoding="utf-8")
		run_case("optional-storage/" + name, [*DEPENDENCIES, path], compiler=compiler)
	for name, source, reject in REJECTIONS:
		directory = ROOT / "build/compiler-new/optional-storage" / name
		directory.mkdir(parents=True, exist_ok=True)
		path = directory / "case.rt"
		path.write_text(source.strip() + "\n", encoding="utf-8")
		run_case("optional-storage/" + name, [*DEPENDENCIES, path], reject=reject, compiler=compiler)
	print(f"optional storage: {len(FIXTURES) + len(CASES) + len(REJECTIONS)} passed")
	return 0


if __name__ == "__main__":
	try:
		sys.exit(main())
	except (OSError, RuntimeError, ValueError) as error:
		print(error, file=sys.stderr)
		sys.exit(1)
