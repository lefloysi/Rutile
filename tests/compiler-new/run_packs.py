"""Compile and execute focused first-class pack regressions, grouped by implementation stage."""

import argparse
from pathlib import Path
import sys

from run import run_case


ROOT = Path(__file__).resolve().parents[2]
STAGES = ("frontend", "core", "expansion", "folds")


def case(stage, name, source, reject=None):
	return (stage, name, source, reject)


CASES = [
	case("core", "default-pack-and-deduction", """
template Types = <typename...>;
template<Types P>
struct Box {};
template<Types P>
fn answer(Box<P>& value) -> i32 { return 42; };
template<Types P = <i32, i64>>
fn defaults() -> i32 { return 42; };
fn main() -> i32 {
	Box<<i32, i64>> value;
	if (defaults<>() != 42) { return 1; };
	return answer(value);
};
"""),
	case("core", "ordinary-template-forward-type", """
namespace support {
	template<typename T>
	struct Box { T* pointer; };
}
struct Container { support::Box<Later> box; };
struct Later { i32 value; };
fn main() -> i32 { return 42; };
"""),
	case("folds", "qualified-fold-name-shadowing", """
template Values = <i32...>;
Values values = <9>;
namespace N { Values values = <20, 22>; }
fn main() -> i32 { return (N::values + ...); };
"""),
	case("frontend", "signature-declarations", """
template Empty = <>;
template Pair = <typename, typename>;
template Types = <typename...>;
template Integers = <i32...>;
template Mixed = <typename, usize>;
fn main() -> i32 { return 42; };
"""),
	case("frontend", "signature-bindings", """
template Empty = <>;
template Pair = <typename, typename>;
template Integers = <i32...>;
Empty empty = <>;
Pair types = <i32, i64>;
Integers values = <i32(10), i32(20)>;
Integers direct_values = <10, 20>;
fn main() -> i32 { return 42; };
"""),
	case("frontend", "duplicate-signature", """
template Types = <typename...>;
template Types = <typename>;
fn main() -> i32 { return 42; };
""", r"(?i)duplicate.*signature"),
	case("frontend", "signature-name-conflict", """
struct Types {};
template Types = <typename...>;
fn main() -> i32 { return 42; };
""", r"(?i)(conflict|previous declaration)"),

	case("frontend", "fixed-prefix-variadic-signature", """
template Nonempty = <typename, typename...>;
Nonempty one = <i32>;
Nonempty three = <i32, i64, f32>;
template Floats = <f32...>;
Floats values = <f32(1.0), f32(2.0)>;
fn main() -> i32 { return 42; };
"""),
	case("frontend", "mixed-literal", """
template Types = <typename...>;
Types bad = <i32, 4>;
fn main() -> i32 { return 42; };
""", r"(?i)(mixed.*(type|value)|(type|value).*mixed|pack literal elements must have.*same argument category)"),
	case("frontend", "fixed-count", """
template Pair = <typename, typename>;
Pair bad = <i32, i64, f32>;
fn main() -> i32 { return 42; };
""", r"(?i)(signature|pack).*(2|two).*(3|three)"),
	case("frontend", "empty-is-exact", """
template Empty = <>;
Empty bad = <i32>;
fn main() -> i32 { return 42; };
""", r"(?i)(signature|pack).*(0|zero).*(1|one)"),
	case("frontend", "wrong-category", """
template Types = <typename...>;
Types bad = <i32(1), i32(2)>;
fn main() -> i32 { return 42; };
""", r"(?i)(expected.*type.*(value|received)|pack.*(kind|category|type))"),
	case("frontend", "wrong-value-type", """
template Integers = <i32...>;
Integers bad = <true, false>;
fn main() -> i32 { return 42; };
""", r"(?i)(expected.*i32|i32.*(bool|received)|pack.*value.*type)"),
	case("frontend", "duplicate-signature", """
template Types = <typename...>;
template Types = <i32...>;
fn main() -> i32 { return 42; };
""", r"(?i)(duplicate.*Types|Types.*already declared|conflicting.*Types)"),
	case("frontend", "signature-conflicts-with-variable", """
template Types = <typename...>;
i32 Types = 42;
fn main() -> i32 { return 42; };
""", r"(?i)(duplicate.*Types|Types.*already declared|conflicting.*Types)"),
	case("core", "independent-pack-parameters", """
template Types = <typename...>;
template Integers = <i32...>;
Types types = <i32, i64>;
Integers values = <i32(1), i32(2), i32(3)>;
template<Types T, typename Middle, Integers V>
fn answer() -> i32 { return 42; };
fn main() -> i32 { return answer<types, bool, values>(); };
"""),
	case("core", "inline-pack-parameter", """
template<<typename...> T>
fn answer() -> i32 { return 42; };
fn main() -> i32 { return answer<<i32, i64>>(); };
"""),
	case("core", "traditional-variadic", """
template<typename... T>
fn answer() -> i32 { return 42; };
fn main() -> i32 {
	if (answer<>() != 42) { return 1; };
	return answer<i32, i64>();
};
"""),
	case("core", "variadic-must-be-last", """
template<typename... T, usize N>
fn invalid() -> i32 { return 42; };
fn main() -> i32 { return 42; };
""", r"(?i)variadic.*(last|final)"),
	case("core", "no-implicit-expansion", """
template Types = <typename...>;
Types types = <i32, i64>;
template<typename T, typename U>
fn answer() -> i32 { return 42; };
fn main() -> i32 { return answer<types>(); };
""", r"(?i)(template.*(argument|expects|count)|expected.*type|pack.*type)"),
	case("core", "local-qualified-pack", """
namespace packs {
	template Types = <typename...>;
	Types types = <i32, i64>;
};
template<packs::Types T>
fn answer() -> i32 { return 42; };
fn main() -> i32 {
	packs::Types local = <i32, i64>;
	if (answer<local>() != 42) { return 1; };
	return answer<packs::types>();
};
"""),
	case("core", "structural-pack-identity", """
template Types = <typename...>;
Types types = <i32, i64>;
template<Types T>
struct Box {};
fn accept(Box<<i32, i64>>& box) -> i32 { return 42; };
fn main() -> i32 { Box<types> box; return accept(box); };
"""),
	case("core", "pack-order-distinct", """
template Types = <typename...>;
Types types = <i64, i32>;
template<Types T>
struct Box {};
fn accept(Box<<i32, i64>>& box) -> i32 { return 42; };
fn main() -> i32 { Box<types> box; return accept(box); };
""", r"(?i)(no applicable function|no.*matching|cannot.*(convert|bind)|argument.*type)"),
	case("core", "value-pack-identity", """
template Integers = <i32...>;
Integers values = <10, 20>;
template<Integers V>
struct Box {};
fn accept(Box<<10, 20>>& box) -> i32 { return 42; };
fn main() -> i32 { Box<values> box; return accept(box); };
"""),
	case("core", "pack-values-distinct", """
template Integers = <i32...>;
Integers values = <10, 21>;
template<Integers V>
struct Box {};
fn accept(Box<<10, 20>>& box) -> i32 { return 42; };
fn main() -> i32 { Box<values> box; return accept(box); };
""", r"(?i)(no applicable function|no.*matching|cannot.*(convert|bind)|argument.*type)"),
	case("core", "signature-not-runtime-type", """
template Types = <typename...>;
fn invalid(Types value);
fn main() -> i32 { return 42; };
""", r"(?i)(signature.*(type|runtime)|does not denote a type|not.*(ordinary|runtime).*type)"),
	case("core", "pack-not-runtime-argument", """
template Integers = <i32...>;
Integers values = <42>;
fn target(i32 value) -> i32 { return value; };
fn main() -> i32 { return target(values); };
""", r"(?i)(pack.*(runtime|value|ordinary)|does not denote a value|no applicable function)"),

	case("expansion", "member-pack-call", """
template<typename...Ts>
struct Fields {
	Ts... values;
};
fn answer(i32 first, i64 second) -> i32 { return first + i32(second); };
fn main() -> i32 {
	var fields = Fields<i32, i64>(20, i64(22));
	return answer(fields.values...);
};
"""),

	case("expansion", "member-pack-comma-fold", """
i32 sequence;
fn observe(i32 value) { @sequence = sequence * 10 + value; };
fn observe(i64 value) { @sequence = sequence * 10 + i32(value); };
template<typename...Ts>
struct Fields { Ts... values; };
fn main() -> i32 {
	var fields = Fields<i32, i64>(4, i64(2));
	(observe(fields.values), ...);
	return sequence;
};
"""),


	case("expansion", "member-pack-private-access", """
template<typename...Ts>
struct Fields {
private:
	Ts... values;
};
fn answer(i32 value) -> i32 { return value; };
fn main() -> i32 { Fields<i32> fields; return answer(fields.values...); };
""", r"member pack is not accessible from this scope"),

	case("expansion", "member-pack-private-empty-fold", """
template<typename...Ts>
struct Fields {
private:
	Ts... values;
};
fn main() -> i32 {
	Fields<> fields;
	(fields.values, ...);
	return 42;
};
""", r"member pack is not accessible from this scope"),
	case("expansion", "member-pack-union-null-value", """
template<typename...Ts>
union Storage { Ts... values; };
fn answer(void* value) -> i32 { if (value == nullptr) { return 42; }; return 1; };
fn main() -> i32 {
	Storage<null> storage;
	return answer(storage.values...);
};
"""),

	case("expansion", "member-pack-immutable-receiver", """
template<typename...Ts>
struct Fields { Ts... values; };
fn mutate(var i32& value) { @value = 42; };
fn main() -> i32 {
	const Fields<i32> fields;
	mutate(@fields.values...);
	return 42;
};
""", r"(?i)(mutable|const|immutable|modif|qualif)"),
	case("expansion", "member-pack-unqualified-fold", """
template<typename...Ts>
struct Fields {
	Ts... values;
	fn answer(Fields& this) -> i32 { return i32((values + ...)); };
};
fn main() -> i32 {
	var fields = Fields<i32, i32>(20, 22);
	return fields.answer();
};
"""),

	case("expansion", "member-pack-layout-and-empty", """
template<typename...Ts>
struct Fields {
	u08 before;
	Ts... values;
	u08 after;
};
template<typename...Ts>
union Storage {
	Ts... values;
};
fn main() -> i32 {
	if (sizeof(Fields<>) != 2 || sizeof(Fields<i32>) != 12) { return 1; };
	if (sizeof(Fields<i64, i32>) != 24 || sizeof(Storage<i32, i64>) != 8) { return 2; };
	if (sizeof(Storage<null, i64>) != 8) { return 3; };
	return 42;
};
"""),
	case("expansion", "member-pack-empty-call", """
template<typename...Ts>
struct Fields { Ts... values; };
fn answer(i32 first, i32 last) -> i32 { return first + last; };
fn main() -> i32 {
	Fields<> fields;
	return answer(20, fields.values..., 22);
};
"""),

	case("expansion", "member-pack-forward-type", """
template Types = <typename...>;
Types types = <Later>;
struct Holder {
	u08 before;
	types... values;
	u08 after;
};
struct Later { i32 value; };
fn main() -> i32 { if (sizeof(Holder) != 12) { return 1; }; return 42; };
"""),


	case("expansion", "member-pack-incomplete-type", """
struct Later;
template Types = <typename...>;
Types types = <Later>;
struct Holder { types... values; };
fn main() -> i32 { return 42; };
""", r"member pack element requires a complete nonrecursive storage type"),

	case("expansion", "member-pack-lifetime", """
i32 constructed;
i32 destroyed;
struct Tracked {
	fn Tracked(var Tracked& this) { @constructed = constructed + 1; };
	fn ~Tracked(var Tracked& this) { @destroyed = destroyed + 1; };
};
template<typename...Ts>
struct Fields { Ts... values; };
fn main() -> i32 {
	{
		Fields<Tracked, Tracked> fields;
		if (constructed != 2 || destroyed) { return 1; };
	};
	if (destroyed != 2) { return 2; };
	return 42;
};
"""),
	case("expansion", "member-pack-nonpack", """
template<typename T>
struct Fields { T... values; };
fn main() -> i32 { Fields<i32> fields; return 42; };
""", r"(?i)(cannot expand|expansion.*pack|expand.*non.pack|expected.*pack)"),
	case("expansion", "member-pack-void", """
template<typename...Ts>
struct Fields { Ts... values; };
fn main() -> i32 { Fields<void> fields; return 42; };
""", r"(?i)(void|object.*storage|storage.*object)"),
	case("expansion", "member-pack-null-struct", """
template<typename...Ts>
struct Fields { Ts... values; };
fn main() -> i32 { Fields<null> fields; return 42; };
""", r"(?i)(null|object.*storage|storage.*object)"),

	case("expansion", "template-interleaved", """
template Types = <typename...>;
Types left = <i32, i64>;
Types right = <f32>;
Types empty = <>;
template<typename A, typename B, typename C, typename D, typename E>
fn answer() -> i32 {
	if (sizeof(A) != 1 || sizeof(B) != 4 || sizeof(C) != 8) { return 1; };
	if (sizeof(D) != 4 || sizeof(E) != 8) { return 2; };
	return 42;
};
fn main() -> i32 { return answer<bool, left..., empty..., right..., f64>(); };
"""),
	case("expansion", "deferred-template-expansion", """
template Types = <typename...>;
template<typename A, typename B>
fn target() -> i32 { return 42; };
template<Types T>
fn forward() -> i32 { return target<T...>(); };
fn main() -> i32 { return forward<<i32, i64>>(); };
"""),
	case("expansion", "call-interleaved", """
template Integers = <i32...>;
Integers left = <i32(1), i32(2)>;
Integers right = <i32(4)>;
Integers empty = <>;
fn target(i32 a, i32 b, i32 c, i32 d, i32 e) -> i32 {
	if (a != 1 || b != 2 || c != 3 || d != 4 || e != 5) { return 1; };
	return 42;
};
fn main() -> i32 { return target(empty..., left..., 3, right..., empty..., 5); };
"""),
	case("expansion", "deferred-call-expansion", """
template Integers = <i32...>;
fn target(i32 a, i32 b) -> i32 { return a + b; };
template<Integers V>
fn forward() -> i32 { return target(V...); };
fn main() -> i32 { return forward<<i32(10), i32(32)>>(); };
"""),
	case("expansion", "expand-non-pack", """
fn target(i32 value) -> i32 { return value; };
fn main() -> i32 { i32 value = 42; return target(value...); };
""", r"(?i)(cannot expand|expansion.*pack|expand.*non.pack|expected.*compile.time pack)"),
	case("expansion", "expanded-call-count", """
template Integers = <i32...>;
Integers values = <1, 2>;
fn target(i32 value) -> i32 { return value; };
fn main() -> i32 { return target(values...); };
""", r"(?i)(argument.*(count|expects|expected|many)|expects.*argument|no.*matching|no applicable function)"),
	case("expansion", "empty-template-receiver-count", """
template Types = <typename...>;
Types empty = <>;
template<typename T>
fn target() -> i32 { return 42; };
fn main() -> i32 { return target<empty...>(); };
""", r"(?i)(template.*(argument|expects|count)|argument.*(expected|missing))"),
	case("expansion", "float-values", """
template Floats = <f32...>;
Floats values = <f32(1.0), f32(2.0)>;
fn add(f32 a, f32 b) -> f32 { return a + b; };
template<f32 V>
fn constant() -> f32 { return V; };
template<Floats V>
fn forward() -> f32 { return add(V...); };
fn main() -> i32 {
	if (add(values...) != f32(3.0)) { return 1; };
	if (forward<<f32(1.0), f32(2.0)>>() != f32(3.0)) { return 2; };
	if (constant<f32(2.0)>() != f32(2.0)) { return 3; };
	return 42;
};
"""),
	case("expansion", "computed-constant-values", """
template Floats = <f32...>;
template Integers = <i32...>;
template Booleans = <bool...>;
const f32 base = f32(1.5);
fn make() -> f32 { return f32(1.5); };
Floats values = <base + f32(1.0), make()>;
Integers bounds = <i32::max>;
Booleans flags = <(true && !false), (i32::max > 0)>;
fn floats(f32 a, f32 b) -> bool { return a == f32(2.5) && b == f32(1.5); };
fn integers(i32 value) -> bool { return value == 2147483647; };
fn booleans(bool a, bool b) -> bool { return a && b; };
fn main() -> i32 {
	if (!floats(values...) || !integers(bounds...) || !booleans(flags...)) { return 1; };
	return 42;
};
"""),
	case("core", "deduced-nested-variadic", """
template<typename... T>
struct Values {};
template<typename... T>
fn answer(Values<i32, T...>& value) -> i32 { return 42; };
fn main() -> i32 {
	Values<i32, i64, f32> many;
	Values<i32> one;
	if (answer(many) != 42) { return 1; };
	return answer(one);
};
"""),
	case("core", "pack-member-definition", """
template Types = <typename...>;
template<Types P>
struct Box {
	fn answer(Box& this) -> i32;
};
template<Types P>
fn Box<P>::answer(Box& this) -> i32 { return 42; };
fn main() -> i32 {
	Box<<i32, i64>> box;
	return box.answer();
};
"""),
	case("folds", "addition", """
template Integers = <i32...>;
template<Integers V>
fn sum() -> i32 { return (V + ...); };
fn main() -> i32 { return sum<<i32(10), i32(12), i32(20)>>(); };
"""),
	case("folds", "logical-identities", """
template Booleans = <bool...>;
template<Booleans V>
fn all() -> bool { return (V && ...); };
template<Booleans V>
fn any() -> bool { return (V || ...); };
fn main() -> i32 {
	if (!all<<>>() || any<<>>()) { return 1; };
	if (!all<<true, true>>() || all<<true, false>>()) { return 2; };
	if (!any<<false, true>>() || any<<false, false>>()) { return 3; };
	return 42;
};
"""),
	case("folds", "type-predicate", """
template<typename T, typename U>
fn same_size() -> bool { return sizeof(T) == sizeof(U); };
template<typename T, typename... U>
fn contains_size() -> bool { return (same_size<T, U>() || ...); };
fn main() -> i32 {
	if (!contains_size<i32, bool, i64, f32>()) { return 1; };
	if (contains_size<i32, bool, i64>() || contains_size<i32>()) { return 2; };
	return 42;
};
"""),
	case("folds", "comma-order-and-short-circuit", """
var i32 sequence = 0;
template<i32 V>
fn record() -> i32 { @sequence = sequence * 10 + V; return V; };
template<i32 V>
fn positive() -> bool { @sequence = sequence * 10 + V; return V != 0; };
template<i32... V>
fn visit() -> i32 { return (record<V>(), ...); };
template<i32... V>
fn every() -> bool { return (positive<V>() && ...); };
template<i32... V>
fn some() -> bool { return (positive<V>() || ...); };
template<i32... V>
fn visit_empty() { (record<V>(), ...); };
fn main() -> i32 {
	if (visit<i32(1), i32(2), i32(3)>() != 3 || sequence != 123) { return 1; };
	@sequence = 0;
	if (every<i32(1), i32(0), i32(3)>() || sequence != 10) { return 2; };
	@sequence = 0;
	if (!some<i32(0), i32(2), i32(3)>() || sequence != 2) { return 3; };
	@sequence = 42;
	visit_empty<>();
	return sequence;
};
"""),
	case("folds", "nested-variadic-prefix-deduction", """
template<typename... Ts>
struct List {};
template<typename T>
fn is_eight() -> bool { return sizeof(T) == 8; };
template<typename... Ts>
fn has_eight(List<i32, Ts...>& value) -> bool { return (is_eight<Ts>() || ...); };
fn main() -> i32 {
	List<i32, i64, f32> many;
	List<i32> empty;
	if (!has_eight(many) || has_eight(empty)) { return 1; };
	return 42;
};
"""),
	case("core", "nested-variadic-repeated-mismatch", """
template<typename... Ts>
struct List {};
template<typename... Ts>
fn compare(List<i32, Ts...>& first, List<i32, Ts...>& second) -> i32 { return 42; };
fn main() -> i32 {
	List<i32, i64> first;
	List<i32, f32> second;
	return compare(first, second);
};
""", r"(?i)(deduc|matching|candidate|overload)"),

	case("folds", "template-variable-membership", """
template<typename T, typename U>
fn same_size() -> bool { return sizeof(T) == sizeof(U); };
template<typename T, typename... Ts>
bool contains = (same_size<T, Ts>() || ...);
template<typename T>
fn present() -> bool { return true; };
template<typename T, typename... Ts>
const bool constant_contains = (present<Ts>() || ...);
template<bool V>
fn answer() -> i32 { if (V) { return 42; }; return 1; };
fn main() -> i32 {
	if (contains<i32> || contains<i32, i64, f64>) { return 1; };
	if (!contains<i32, i64, f32>) { return 2; };
	if (answer<!constant_contains<i32>>() != 42) { return 3; };
	return answer<constant_contains<i32, i64, f32>>();
};
"""),

	case("folds", "right-associative-subtraction", """
template Integers = <i32...>;
Integers values = <10, 3, 2>;
fn main() -> i32 { return (values - ...) + 33; };
"""),
	case("folds", "literal-addition", """
fn main() -> i32 { return (<10, 12, 20> + ...); };
"""),
	case("folds", "qualified-pack", """
namespace numbers {
	template Integers = <i32...>;
	Integers values = <10, 12, 20>;
};
fn main() -> i32 { return (numbers::values + ...); };
"""),
	case("folds", "empty-addition-rejected", """
template<i32... V>
fn sum() -> i32 { return (V + ...); };
fn main() -> i32 { return sum<>(); };
""", r"(?i)(empty.*fold|fold.*empty|identity.*\+)"),
]


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", type=Path)
	parser.add_argument("--stage", choices=(*STAGES, "all"), default="all",
		help="Run exactly one stage, or all stages")
	options = parser.parse_args()
	compiler = options.compiler.resolve() if options.compiler else None
	count = 0
	for stage, name, source, reject in CASES:
		if options.stage != "all" and stage != options.stage:
			continue
		directory = ROOT / "build/compiler-new/packs" / name
		directory.mkdir(parents=True, exist_ok=True)
		path = directory / "case.rt"
		path.write_text(source.strip() + "\n", encoding="utf-8")
		run_case("packs/" + name, [path], reject=reject, compiler=compiler)
		count += 1
	print(f"packs ({options.stage}): {count} passed")
	return 0


if __name__ == "__main__":
	try:
		sys.exit(main())
	except (OSError, RuntimeError, ValueError) as error:
		print(error, file=sys.stderr)
		sys.exit(1)
