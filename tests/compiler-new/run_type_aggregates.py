"""Compile and execute type-level tuple and variant regressions."""

import argparse
from pathlib import Path
import sys

from run import run_case


ROOT = Path(__file__).resolve().parents[2]
FIXTURE = """
struct Foo {};
struct Bar {};
struct Baz {};
"""


CASES = [

	("inline-record-local-global", """
struct { i32 value; } global_value;
fn main() -> i32 {
	var struct { i32 value; } local_value;
	@local_value.value = 42;
	if (sizeof(local_value) != 4 || global_value.value) { return 1; };
	return local_value.value;
};
""", None),
	("inline-union-member-layout", """
struct Holder {
	u08 before;
	union { i32 small; i64 large; } storage;
	u08 after;
};
fn main() -> i32 {
	var Holder holder;
	@holder.storage.large = i64(42);
	if (sizeof(Holder) != 24) { return 1; };
	return i32(holder.storage.large);
};
""", None),
	("inline-record-nested", """
fn main() -> i32 {
	var struct { struct { i32 value; } inner; } outer;
	@outer.inner.value = 42;
	return outer.inner.value;
};
""", None),
	("inline-record-postfix-types", """
fn main() -> i32 {
	const struct { i32 value; }* pointer = nullptr;
	var struct { i32 value; }[2] entries;
	@entries[1].value = 42;
	if (pointer != nullptr || sizeof(entries) != 8) { return 1; };
	return entries[1].value;
};
""", None),
	("inline-union-generic-member-pack", """
template<typename...Ts>
struct Box {
	union { Ts... values; } storage;
};
fn answer(void* value) -> i32 { if (value == nullptr) { return 42; }; return 1; };
fn main() -> i32 {
	Box<null> box;
	if (sizeof(Box<i32, i64>) != 8) { return 1; };
	return answer(box.storage.values...);
};
""", None),

	("inline-record-generic-type-contexts", """
using Value = struct { i32 value; };
template Types = <typename...>;
Types types = <struct { i32 value; }, union { i64 value; }>;
template<typename T>
fn width() -> usize { return sizeof(T); };
template<typename T, typename U>
fn answer() -> i32 {
	if (sizeof(T) != 4 || sizeof(U) != 8) { return 1; };
	return 42;
};
fn main() -> i32 {
	var value = Value(42);
	if (value.value != 42 || width<struct { i64 value; }>() != 8) { return 1; };
	return answer<types...>();
};
""", None),

	("inline-record-template-identity", """
template<typename T>
struct Box {
	struct { T value; } inner;
	struct Inner { T value; } first;
	Inner second;
};
fn main() -> i32 {
	var Box<i32> small;
	Box<i64> large;
	@small.inner.value = 42;
	@small.first.value = 21;
	@small.second := small.first;
	var copied = small.inner;
	@small.inner := copied;
	if (sizeof(small) != 12 || sizeof(large) != 24 || small.second.value != 21) { return 1; };
	return copied.value;
};
""", None),

	("inline-record-template-exception-identity", """
template<typename T>
struct Box { using Error = struct { T value; }; };
template<typename T>
fn raise() {
	var error = Box<T>::Error(T(42));
	throw $error;
};
fn main() -> i32 {
	try { raise<i32>(); }
	catch (Box<i64>::Error& wrong) { return 1; }
	catch (Box<i32>::Error& error) { return error.value; };
	return 2;
};
""", None),

	("inline-record-distinct-identity", """
fn main() -> i32 {
	var struct { i32 value; } first;
	struct { i32 value; } second;
	@first := second;
	return 42;
};
""", r"(?i)(no applicable|cannot|convert|different|mismatch)"),
	("inline-record-lifetime", """
i32 constructed;
i32 copies;
i32 destroyed;
struct Tracked {
	fn Tracked(var Tracked& this) { @constructed = constructed + 1; };
	fn Tracked(var Tracked& this, Tracked& other) { @copies = copies + 1; };
	fn Tracked(var Tracked& this, Tracked other) : [default];
	fn ~Tracked(var Tracked& this) { @destroyed = destroyed + 1; };
};
fn main() -> i32 {
	{
		struct { Tracked value; } first;
		var copied = first;
		var moved = $copied;
		if (constructed != 1 || copies != 1 || destroyed) { return 1; };
	};
	if (destroyed != 2) { return 2; };
	return 42;
};
""", None),
	("inline-class-private-access", """
fn main() -> i32 {
	class { private: i32 value; }* object = nullptr;
	return object->value;
};
""", r"not accessible from this scope"),
	("inline-class-public-access", """
fn main() -> i32 {
	var class { public: i32 value; } object;
	@object.value = 42;
	return object.value;
};
""", None),
	("tuple-declaration", """
struct Holder { Foo & Bar value; };
fn accept(rt::tuple<Foo, Bar>* value) -> i32 { return 42; };
fn inspect(Holder& value) -> i32 { return accept(&value.value); };
fn main() -> i32 { return 42; };
""", None),
	("variant-declaration", """
struct Holder { Foo | Bar value; };
fn accept(rt::variant<Foo, Bar>* value) -> i32 { return 42; };
fn inspect(Holder& value) -> i32 { return accept(&value.value); };
fn main() -> i32 { return 42; };
""", None),
	("tuple-chain", """
using Triple = Foo & Bar & Baz;
fn accept(rt::tuple<Foo, Bar, Baz>* value) -> i32 { return 42; };
fn main() -> i32 { Triple* value = nullptr; return accept(value); };
""", None),
	("variant-chain", """
using Choice = Foo | Bar | Baz;
fn accept(rt::variant<Foo, Bar, Baz>* value) -> i32 { return 42; };
fn main() -> i32 { Choice* value = nullptr; return accept(value); };
""", None),
	("left-grouped-variant", """
using Choice = (Foo | Bar) | Baz;
fn accept(rt::variant<rt::variant<Foo, Bar>, Baz>* value) -> i32 { return 42; };
fn main() -> i32 { Choice* value = nullptr; return accept(value); };
""", None),
	("tuple-identity", """
fn accept(rt::tuple<Foo, Bar>* value) -> i32 { return 42; };
fn main() -> i32 { (Foo & Bar)* value = nullptr; return accept(value); };
""", None),
	("variant-identity", """
fn accept(rt::variant<Foo, Bar>* value) -> i32 { return 42; };
fn main() -> i32 { (Foo | Bar)* value = nullptr; return accept(value); };
""", None),
	("tuple-alias", """
using Pair = Foo & Bar;
fn accept(rt::tuple<Foo, Bar>* value) -> i32 { return 42; };
fn main() -> i32 { Pair* value = nullptr; return accept(value); };
""", None),
	("variant-alias", """
using Choice = Foo | Bar;
fn accept(rt::variant<Foo, Bar>* value) -> i32 { return 42; };
fn main() -> i32 { Choice* value = nullptr; return accept(value); };
""", None),
	("nested-variant", """
using Choice = Foo | (Bar | Baz);
fn accept(rt::variant<Foo, rt::variant<Bar, Baz>>* value) -> i32 { return 42; };
fn main() -> i32 { Choice* value = nullptr; return accept(value); };
""", None),
	("nested-tuple", """
using Pair = Foo & (Bar & Baz);
fn accept(rt::tuple<Foo, rt::tuple<Bar, Baz>>* value) -> i32 { return 42; };
fn main() -> i32 { Pair* value = nullptr; return accept(value); };
""", None),
	("mixed-precedence", """
using Choice = Foo | Bar & Baz;
fn accept(rt::variant<Foo, rt::tuple<Bar, Baz>>* value) -> i32 { return 42; };
fn main() -> i32 { Choice* value = nullptr; return accept(value); };
""", None),
	("mixed-grouping", """
using Pair = (Foo | Bar) & Baz;
fn accept(rt::tuple<rt::variant<Foo, Bar>, Baz>* value) -> i32 { return 42; };
fn main() -> i32 { Pair* value = nullptr; return accept(value); };
""", None),
	("template-argument", """
template<typename T>
struct Box {};
fn accept(Box<rt::variant<Foo, Bar>>* value) -> i32 { return 42; };
fn main() -> i32 { Box<(Foo | Bar)>* value = nullptr; return accept(value); };
""", None),
	("dependent-alias", """
template<typename T, typename U>
struct Box {
	using Choice = T | U;
	Choice value;
};
fn accept(rt::variant<Foo, Bar>* value) -> i32 { return 42; };
fn inspect(Box<Foo, Bar>& value) -> i32 { return accept(&value.value); };
fn main() -> i32 { return 42; };
""", None),
	("value-bitwise", """
fn main() -> i32 {
	i32 first = 40;
	i32 second = 2;
	if ((first & second) != 0) { return 1; };
	return first | second;
};
""", None),
	("ordered-identity", """
fn accept(rt::variant<Foo, Bar>* value) -> i32 { return 42; };
fn main() -> i32 { (Bar | Foo)* value = nullptr; return accept(value); };
""", r"(?i)(matching|candidate|overload|convert|argument)"),
]


LIBRARY_CASES = [
	("variant-proof-false-two-alternatives", """
struct Error {};
using Choice = void* | Error;
fn take(void* value) -> i32 { return 42; };
fn inspect(Choice& value) -> i32 {
	if (value.has<Error>()) { return 1; }
	else { return take(value); };
};
fn main() -> i32 { void* pointer = nullptr; Choice value = Choice(pointer); return inspect(value); };
""", None),
	("variant-proof-guard-return", """
struct Error {};
using Choice = void* | Error;
fn take(void* value) -> i32 { return 42; };
fn inspect(Choice& value) -> i32 {
	if (!value.has<void*>()) { return 1; };
	return take(value);
};
fn main() -> i32 { void* pointer = nullptr; Choice value = Choice(pointer); return inspect(value); };
""", None),
	("variant-proof-false-three-stays-unknown", """
struct Error {};
using Choice = void* | Error | i64;
fn take(void* value) -> i32 { return 42; };
fn inspect(Choice& value) -> i32 {
	if (value.has<Error>()) { return 1; };
	return take(value);
};
fn main() -> i32 { return 42; };
""", r"(?i)(cannot|applicable|argument|convert)"),

	("variant-explicit-cast-establishes-proof", """
struct Error {};
using Choice = void* | Error;
fn take(void* value) -> i32 { if (value != nullptr) { return 42; }; return 1; };
fn inspect(Choice& value) -> i32 {
	(void*)value;
	return take(value);
};
fn main() -> i32 {
	i32 number = 1;
	void* pointer = &number;
	Choice value = Choice(pointer);
	return inspect(value);
};
""", None),
	("variant-bare-cast-success", """
using Choice = i32 | i64;
fn inspect(Choice& value) -> i32 { (i32)value; return 42; };
fn main() -> i32 { Choice value = Choice(42); return inspect(value); };
""", None),
	("variant-bare-cast-mismatch", """
using Choice = i32 | i64;
fn main() -> i32 {
	Choice value = Choice(i64(42));
	try { (i32)value; }
	catch (i64 held) { return i32(held); };
	return 1;
};
""", None),

	("variant-proof-null-before-pointer", """
struct Error {};
using Choice = null | void* | Error;
fn take(void* value) -> i32 { return 42; };
fn inspect(Choice& value) -> i32 {
	if (value.has<void*>()) { return take(value); };
	return 1;
};
fn main() -> i32 {
	void* pointer = nullptr;
	Choice value = Choice(pointer);
	return inspect(value);
};
""", None),
	("variant-proof-null-branch", """
using Choice = null | i64;
fn take(void* value) -> i32 { if (value == nullptr) { return 42; }; return 1; };
fn inspect(Choice& value) -> i32 {
	if (value.has<null>()) { return take(value); };
	return 1;
};
fn main() -> i32 { Choice value; return inspect(value); };
""", None),
	("variant-proof-null-default", """
using Choice = null | i64;
fn take(void* value) -> i32 { if (value == nullptr) { return 42; }; return 1; };
fn main() -> i32 { Choice value; return take(value); };
""", None),


	("variant-proof-first-pointer-default", """
using Choice = void* | i64;
fn take(void* value) -> i32 { if (value == nullptr) { return 42; }; return 1; };
fn main() -> i32 { Choice value; return take(value); };
""", None),
	("variant-proof-middle-null-not-default", """
using Choice = i64 | null;
fn take(void* value) -> i32 { return 42; };
fn main() -> i32 { Choice value; return take(value); };
""", r"(?i)(cannot|applicable|argument|convert)"),

	("variant-proven-cast-noncopyable-other", """
struct NoCopy {
	fn NoCopy(var NoCopy& this) : [default];
	fn NoCopy(var NoCopy& this, NoCopy& other) : [delete];
	fn NoCopy(var NoCopy& this, NoCopy other) : [default];
};
using Choice = i32 | NoCopy;
fn main() -> i32 { Choice value = Choice(42); return (i32)value; };
""", None),
	("variant-proof-short-circuit-invalidates", """
using Choice = void* | i64;
fn take(void* value) -> i32 { return 42; };
fn main() -> i32 {
	void* pointer = nullptr;
	var Choice value = Choice(i64(9));
	false && ((@value = Choice(pointer)).has<void*>());
	return take(value);
};
""", r"(?i)(cannot|applicable|argument|convert)"),

	("variant-proof-operators", """
using Choice = i32 | i64;
fn main() -> i32 {
	Choice value = Choice(40);
	if (!value.has<i32>()) { return 1; };
	if (-value != -40) { return 2; };
	return value + 2;
};
""", None),
	("variant-proof-member", """
struct Payload { i32 answer; };
using Choice = Payload | i64;
fn main() -> i32 { Choice value = Choice(Payload(42)); return value.answer; };
""", None),
	("variant-proof-assignment-invalidates", """
using Choice = void* | i64;
fn take(void* value) -> i32 { return 42; };
fn main() -> i32 {
	void* pointer = nullptr;
	var Choice value = Choice(pointer);
	@value = Choice(i64(9));
	return take(value);
};
""", r"(?i)(cannot|applicable|argument|convert)"),
	("variant-proof-call-invalidates", """
using Choice = void* | i64;
fn take(void* value) -> i32 { return 42; };
fn mutate(var Choice& value) { @value = Choice(i64(9)); };
fn main() -> i32 {
	void* pointer = nullptr;
	var Choice value = Choice(pointer);
	mutate(@value);
	return take(value);
};
""", r"(?i)(cannot|applicable|argument|convert)"),
	("variant-proof-branch-merge", """
using Choice = void* | i64;
fn take(void* value) -> i32 { return 42; };
fn inspect(Choice& value) -> i32 {
	if (value.has<void*>()) { take(value); };
	return take(value);
};
fn main() -> i32 { return 42; };
""", r"(?i)(cannot|applicable|argument|convert)"),
	("variant-proof-direct-grouped-construction", """
struct Error {};
fn take(void* value) -> i32 { return 42; };
fn main() -> i32 {
	void* pointer = nullptr;
	var value = (void* | Error)(pointer);
	return take(value);
};
""", None),

	("variant-null-default", """
using Choice = i32 | null;
fn main() -> i32 {
	Choice value;
	if (!value.has<i32>() || value.has<null>()) { return 1; };
	if (value.get<i32>() != 0) { return 2; };
	return 42;
};
""", None),
	("variant-null-cast", """
using Choice = i32 | null;
fn main() -> i32 {
	Choice value = Choice(nullptr);
	if ((null)value != nullptr) { return 1; };
	return 42;
};
""", None),
	("variant-null-cast-throws", """
using Choice = i32 | null;
fn main() -> i32 {
	Choice value = Choice(nullptr);
	try { i32 discarded = (i32)value; }
	catch (const null* held) { if (held == nullptr) { return 42; }; };
	return 1;
};
""", None),
	("variant-construction-proof", """
struct Error {};
fn take(void* value) -> i32 { if (value != nullptr) { return 42; }; return 1; };
fn main() -> i32 {
	i32 number = 1;
	void* pointer = &number;
	var value = (void* | Error)(pointer);
	return take(value);
};
""", None),
	("variant-branch-proof", """
struct Error {};
using Choice = void* | Error;
fn take(void* value) -> i32 { if (value != nullptr) { return 42; }; return 1; };
fn inspect(Choice& value) -> i32 {
	if (value.has<void*>()) { return take(value); };
	return 1;
};
fn main() -> i32 { i32 number = 1; void* pointer = &number; Choice value = Choice(pointer); return inspect(value); };
""", None),
	("variant-unknown-conversion", """
struct Error {};
using Choice = void* | Error;
fn take(void* value) -> i32 { return 42; };
fn inspect(Choice& value) -> i32 { return take(value); };
fn main() -> i32 { return 42; };
""", r"(?i)(convert|matching|candidate|overload|argument)"),
	("variant-cast-receiver-once", """
using Choice = i32 | i64;
fn source(var Choice& value, var i32& calls) -> var Choice& {
	@calls = calls + 1;
	return @value;
};
fn main() -> i32 {
	var Choice value = Choice(42);
	var i32 calls = 0;
	i32 answer = (i32)source(@value, @calls);
	if (calls != 1) { return 1; };
	return answer;
};
""", None),
	("variant-cast-success", """
using Choice = i32 | i64;
fn main() -> i32 { Choice value = Choice(42); return (i32)value; };
""", None),
	("variant-cast-mutable-reference", """
using Choice = i32 | i64;
fn main() -> i32 {
	var Choice value = Choice(1);
	@((i32)value) = 42;
	return value.get<i32>();
};
""", None),
	("variant-cast-const-reference", """
using Choice = i32 | i64;
fn main() -> i32 {
	const Choice value = Choice(1);
	@((i32)value) = 42;
	return 42;
};
""", r"(?i)(mutable|const|immutable|modif)"),
	("variant-cast-throws-held-scalar", """
using Choice = i32 | i64;
fn main() -> i32 {
	Choice value = Choice(i64(42));
	try { i32 discarded = (i32)value; }
	catch (i64 held) { return i32(held); };
	return 1;
};
""", None),
	("variant-cast-throws-held-record", """
struct Payload { i32 value; };
using Choice = i32 | Payload;
fn main() -> i32 {
	Choice value = Choice(Payload(42));
	try { i32 discarded = (i32)value; }
	catch (Payload& held) { return held.value; };
	return 1;
};
""", None),
	("variant-cast-nested", """
using Inner = i32 | i64;
using Outer = Inner | f64;
fn main() -> i32 {
	Outer value = Outer(Inner(i64(42)));
	try { i32 discarded = (i32)((Inner)value); }
	catch (i64 held) { return i32(held); };
	return 1;
};
""", None),
	("variant-cast-absent-type", """
using Choice = i32 | i64;
fn main() -> i32 { Choice value = Choice(42); return i32((f32)value); };
""", r"variant cast requires a unique alternative"),
	("tuple-values", """
using Pair = i32 & i64;
fn main() -> i32 {
	Pair value = Pair(20, i64(22));
	return value.get<i32>() + i32(value.get<i64>());
};
""", None),
	("tuple-mutable-reference", """
using Pair = i32 & i64;
fn main() -> i32 {
	var Pair value = Pair(1, i64(2));
	@value.get<i32>() = 40;
	return value.get<i32>() + i32(value.get<i64>());
};
""", None),
	("tuple-const-reference", """
using Pair = i32 & i64;
fn main() -> i32 {
	const Pair value = Pair(1, i64(2));
	@value.get<i32>() = 40;
	return 42;
};
""", r"(?i)(mutable|const|immutable|modif)"),
	("tuple-absent-get", """
using Pair = i32 & i64;
fn main() -> i32 { Pair value; return i32(value.get<f32>()); };
""", r"requested type is not an aggregate element"),
	("variant-values", """
using Choice = i32 | i64;
fn main() -> i32 {
	Choice value = Choice(42);
	if (!value.has<i32>() || value.has<i64>()) { return 1; };
	return value.get<i32>();
};
""", None),
	("variant-absent-has", """
using Choice = i32 | i64;
fn main() -> i32 { Choice value = Choice(42); value.has<f32>(); return 42; };
""", r"requested type is not an aggregate element"),
	("variant-absent-get", """
using Choice = i32 | i64;
fn main() -> i32 { Choice value = Choice(42); return i32(value.get<f32>()); };
""", r"requested type is not an aggregate element"),
]


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", type=Path)
	parser.add_argument("--case", help="Run a single named case")
	options = parser.parse_args()
	compiler = options.compiler.resolve() if options.compiler else None
	count = 0
	for name, source, reject in CASES + LIBRARY_CASES:
		if options.case and name != options.case:
			continue
		directory = ROOT / "build/compiler-new/type-aggregates" / name
		directory.mkdir(parents=True, exist_ok=True)
		path = directory / "case.rt"
		library = (name, source, reject) in LIBRARY_CASES
		path.write_text(("" if library else FIXTURE) + source.strip() + "\n", encoding="utf-8")
		sources = [ROOT / "compiler/std/tuple.rt", ROOT / "compiler/std/variant.rt", path]
		run_case("type-aggregates/" + name, sources, reject=reject, compiler=compiler)
		count += 1
	if not count:
		raise ValueError("No matching case")
	print(f"type aggregates: {count} passed")
	return 0


if __name__ == "__main__":
	try:
		sys.exit(main())
	except (OSError, RuntimeError, ValueError) as error:
		print(error, file=sys.stderr)
		sys.exit(1)
