"""Check inferred function facts across actual call boundaries."""

import argparse
from pathlib import Path
import re
import subprocess

from run import run_case


ROOT = Path(__file__).resolve().parents[2]
BOUNDS = "array index is provably outside the array bounds"
CASES = [
	('default-record-narrow-widen', 'struct Counter { usize length = usize(u08(257)); }; fn main() -> i32 { i32[2] data {}; Counter c; return data[c.length]; };', False),
	('default-record-narrow-invalid', 'struct Counter { u08 length = u08(258); }; fn main() -> i32 { i32[2] data {}; Counter c; return data[c.length]; };', True),
	('default-record-many-fields', 'struct Counter { usize field0; usize field1; usize field2; usize field3; usize field4; usize field5; usize field6; usize field7; usize field8; usize field9; usize field10; usize field11; usize field12; usize field13; usize field14; usize field15; usize field16; usize field17; usize field18; usize field19; usize field20; usize field21; usize field22; usize field23; usize field24; usize field25; usize field26; usize field27; usize field28; usize field29; usize field30; usize field31; usize field32; usize field33; usize field34; usize field35; usize field36; usize field37; usize field38; usize field39; usize field40; usize field41; usize field42; usize field43; usize field44; usize field45; usize field46; usize field47; usize field48; usize field49; usize field50; usize field51; usize field52; usize field53; usize field54; usize field55; usize field56; usize field57; usize field58; usize field59; usize field60; usize field61; usize field62; usize field63; }; fn read(Counter& c) -> usize { c.field0; c.field1; c.field2; c.field3; c.field4; c.field5; c.field6; c.field7; c.field8; c.field9; c.field10; c.field11; c.field12; c.field13; c.field14; c.field15; c.field16; c.field17; c.field18; c.field19; c.field20; c.field21; c.field22; c.field23; c.field24; c.field25; c.field26; c.field27; c.field28; c.field29; c.field30; c.field31; c.field32; c.field33; c.field34; c.field35; c.field36; c.field37; c.field38; c.field39; c.field40; c.field41; c.field42; c.field43; c.field44; c.field45; c.field46; c.field47; c.field48; c.field49; c.field50; c.field51; c.field52; c.field53; c.field54; c.field55; c.field56; c.field57; c.field58; c.field59; c.field60; c.field61; c.field62; return c.field63; }; fn main() -> i32 { i32[2] data {}; Counter c; return data[read(c) + 2]; };', True),
	('default-record-zero-getter', 'struct Counter { usize length; }; fn read(Counter& c) -> usize { return c.length; }; fn main() -> i32 { i32[2] data {}; Counter c; return data[read(c) + 2]; };', True),
	('default-record-initializer', 'struct Counter { usize length = 3; }; fn read(Counter& c) -> usize { return c.length; }; fn main() -> i32 { i32[2] data {}; Counter c; return data[read(c)]; };', True),
	('default-record-nested', 'struct Inner { usize length = 3; }; struct Outer { Inner child; }; fn read(Outer& c) -> usize { return c.child.length; }; fn main() -> i32 { i32[2] data {}; Outer c; return data[read(c)]; };', True),
	('default-record-array-field', 'struct Counter { i32[2] data; usize length = 3; }; fn read(Counter& c) -> usize { return c.length; }; fn main() -> i32 { i32[2] data {}; Counter c; return data[read(c)]; };', True),
	('default-record-preserves-scalars', 'struct Counter { usize length; }; fn main() -> i32 { i32[2] data {}; usize index = 3; Counter c; return data[index]; };', True),
	('default-record-custom-constructor', 'struct Counter { usize length = 3; fn Counter(var Counter& this) { @this.length = 0; }; }; fn read(Counter& c) -> usize { return c.length; }; fn main() -> i32 { i32[2] data {}; Counter c; return data[read(c)]; };', False),
	('default-record-nested-custom', 'usize index; struct Inner { fn Inner(var Inner& this) { @index = 0; }; }; struct Outer { Inner child; usize length = 3; }; fn main() -> i32 { i32[2] data {}; @index = 3; Outer c; return data[index]; };', False),
	('default-record-impure-initializer', 'usize index; fn clear() -> usize { @index = 0; return 3; }; struct Counter { usize length = clear(); }; fn main() -> i32 { i32[2] data {}; @index = 3; Counter c; return data[index]; };', False),
	('default-record-reference-boundary', 'usize global; struct Counter { usize& reference = global; usize length = 3; }; fn read(Counter& c) -> usize { return c.length; }; fn main() -> i32 { i32[2] data {}; Counter c; return data[read(c)]; };', False),
	('default-record-variant-boundary', 'using Choice = i32 | i64; struct Counter { Choice choice; usize length = 3; }; fn read(Counter& c) -> usize { return c.length; }; fn main() -> i32 { i32[2] data {}; Counter c; return data[read(c)]; };', False),
	('default-record-overlap-boundary', 'union Counter { usize length; usize other; }; fn main() -> i32 { i32[2] data {}; Counter c; return data[c.length]; };', False),
	('default-record-reconstruction', 'struct Counter { usize length; }; fn read(Counter& c) -> usize { return c.length; }; fn main() -> i32 { i32[2] data {}; Counter c; @c.length = 3; @c.~Counter(); @c.Counter(); return data[read(c) + 2]; };', True),
	('composed-deep-mutating-wrappers', 'fn wrap0(var usize& n, bool choice) -> bool { return wrap1(@n, choice); }; fn wrap1(var usize& n, bool choice) -> bool { return wrap2(@n, choice); }; fn wrap2(var usize& n, bool choice) -> bool { return wrap3(@n, choice); }; fn wrap3(var usize& n, bool choice) -> bool { return wrap4(@n, choice); }; fn wrap4(var usize& n, bool choice) -> bool { return wrap5(@n, choice); }; fn wrap5(var usize& n, bool choice) -> bool { return wrap6(@n, choice); }; fn wrap6(var usize& n, bool choice) -> bool { return wrap7(@n, choice); }; fn wrap7(var usize& n, bool choice) -> bool { return wrap8(@n, choice); }; fn wrap8(var usize& n, bool choice) -> bool { if (choice) { @n = 3; return true; }; return false; }; fn inspect(bool choice) -> i32 { i32[2] data {}; usize n = 1; if (wrap0(@n, choice)) { return data[n]; }; return 0; };', True),
	('composed-path-budget', 'fn choose(var usize& n, bool choice) -> bool { if (choice) { @n = 3; return true; }; @n = 0; return false; }; fn wrap(var usize& n, bool choice) -> bool { choose(@n, choice); choose(@n, choice); choose(@n, choice); choose(@n, choice); choose(@n, choice); choose(@n, choice); choose(@n, choice); return false; }; fn main() -> i32 { i32[2] data {}; usize n = 3; wrap(@n, false); return data[n]; };', False),
	('composed-opaque-callee', 'fn external(var usize* n) -> bool; fn wrap(var usize* n) -> bool { return external(n); }; fn inspect() -> i32 { i32[2] data {}; usize n = 3; wrap(&@n); return data[n]; };', False),
	('composed-narrow-overflow', 'fn increment(var u08& n) -> bool { @n = n + 1; return true; }; fn wrap(var u08& n) -> bool { return increment(@n); }; fn main() -> i32 { i32[2] data {}; u08 n = 255; wrap(@n); return data[n]; };', False),
	('composed-bool-state-invalid', 'fn set(var usize& n, bool choice) -> bool { if (choice) { @n = 3; return true; }; return false; }; fn wrap(var usize& n, bool choice) -> bool { return set(@n, choice); }; fn inspect(bool choice) -> i32 { i32[2] data {}; usize n = 1; bool result = wrap(@n, choice); if (result) { return data[n]; }; return 0; };', True),
	('composed-bool-other-state', 'fn set(var usize& n, bool choice) -> bool { if (choice) { @n = 3; return true; }; return false; }; fn wrap(var usize& n, bool choice) -> bool { bool result = set(@n, choice); return result; }; fn inspect(bool choice) -> i32 { i32[2] data {}; usize n = 1; if (!wrap(@n, choice)) { return data[n]; }; return 0; };', False),
	('composed-conditional-call', 'fn set(var usize& n, bool choice) -> bool { if (choice) { @n = 3; return true; }; return false; }; fn wrap(var usize& n, bool choice) -> bool { if (set(@n, choice)) { return true; }; return false; }; fn inspect(bool choice) -> i32 { i32[2] data {}; usize n = 1; if (wrap(@n, choice)) { return data[n]; }; return 0; };', True),
	('composed-void-write', 'fn set(var usize& n) { @n = 3; }; fn wrap(var usize& n) { set(@n); }; fn main() -> i32 { i32[2] data {}; usize n = 1; wrap(@n); return data[n]; };', True),
	('composed-write-snapshots', 'fn step(var usize& n) -> usize { usize old = n; @n = n + 1; @n = n + 1; return old; }; fn wrap(var usize& n) -> usize { return step(@n); }; fn main() -> i32 { i32[2] data {}; usize n = 0; usize old = wrap(@n); return data[n - old]; };', True),
	('composed-assignment-call', 'fn set(var usize& n, bool choice) -> bool { if (choice) { @n = 3; return true; }; return false; }; fn wrap(var usize& n, bool choice) -> bool { bool result = false; @result = set(@n, choice); return result; }; fn inspect(bool choice) -> i32 { i32[2] data {}; usize n = 1; if (wrap(@n, choice)) { return data[n]; }; return 0; };', True),
	('composed-union-state', 'using Choice = i32 | i64; fn set(var usize& n, bool choice) -> Choice { if (choice) { @n = 3; return Choice(i32(0)); }; return Choice(i64(0)); }; fn wrap(var usize& n, bool choice) -> Choice { return set(@n, choice); }; fn inspect(bool choice) -> i32 { i32[2] data {}; usize n = 1; Choice result = wrap(@n, choice); if (result.has<i32>()) { return data[n]; }; return 0; };', True),
	('composed-union-other-state', 'using Choice = i32 | i64; fn set(var usize& n, bool choice) -> Choice { if (choice) { @n = 3; return Choice(i32(0)); }; return Choice(i64(0)); }; fn wrap(var usize& n, bool choice) -> Choice { return set(@n, choice); }; fn inspect(bool choice) -> i32 { i32[2] data {}; usize n = 1; Choice result = wrap(@n, choice); if (result.has<i64>()) { return data[n]; }; return 0; };', False),
	('composed-nested-call-effects', 'fn clear(var usize& n) -> bool { @n = 0; return true; }; fn choose(bool value) -> bool { return value; }; fn wrap(var usize& n) -> bool { return choose(clear(@n)); }; fn main() -> i32 { i32[2] data {}; usize n = 3; wrap(@n); return data[n]; };', False),
	('composed-alias-unknown', 'fn change(var usize& first, var usize& second) -> bool { @first = 3; @second = 0; return true; }; fn wrap(var usize& n) -> bool { return change(@n, @n); }; fn main() -> i32 { i32[2] data {}; usize n = 3; wrap(@n); return data[n]; };', False),
	('composed-recursion-unknown', 'fn first(var usize& n) -> bool { return second(@n); }; fn second(var usize& n) -> bool { return first(@n); }; fn inspect(var usize& n) -> i32 { i32[2] data {}; if (first(@n)) { return data[n]; }; return 0; };', False),
	('canonical-forward-wrapper', 'fn identity(usize declaration) -> usize; fn identity(usize definition) -> usize { return definition; }; fn wrap(usize value) -> usize { return identity(value); }; fn main() -> i32 { i32[2] data {}; return data[wrap(3)]; };', True),
	('composed-overflow-outcome', 'fn grow(var usize& n) -> bool { if (n == usize::max) { return false; }; @n = n + 1; return true; }; fn wrap(var usize& n) -> bool { return grow(@n); }; fn main() -> i32 { i32[2] data {}; usize n = usize::max; if (wrap(@n)) { return data[n]; }; return 0; };', False),
	("guarded-shrink-invalid", "fn shrink(var usize& value) -> bool { if (value == 0) { return false; }; @value = value - 1; return true; }; fn main() -> i32 { i32[2] data {}; usize index = 3; if (shrink(@index)) { return data[index]; }; return 0; };", True),
	("assignment-address-after-value", "fn main() -> i32 { i32[2] data {}; usize index = 2; @data[index] = i32(@index = 0) + 42; return data[0]; };", False),
	("assignment-value-invalidates-address", "fn main() -> i32 { i32[2] data {}; usize index = 0; @data[index] = i32(@index = 2) + 40; return 42; };", True),
	("array-element-preserves-scalar", "fn main() -> i32 { i32[2] data {}; usize index = 2; @data[0] = 7; return data[index]; };", True),
	("array-unknown-element-preserves-scalar", "fn inspect(usize element) -> i32 { i32[2] data {}; usize index = 2; if (element < 2) { @data[element] = 7; return data[index]; }; return 0; };", True),
	("array-unproven-element-unknown", "fn inspect(usize element) -> i32 { i32[2] data {}; usize index = 2; @data[element] = 7; return data[index]; };", False),
	("array-element-preserves-sibling", "struct Holder { usize length; i32[2] data; }; fn main() -> i32 { i32[2] data {}; Holder holder; @holder.length = 2; @holder.data[0] = 7; return data[holder.length]; };", True),
	("raw-element-write-unknown", "fn main() -> i32 { i32[2] data {}; var i32* pointer = &@data[0]; usize index = 2; @pointer[0] = 7; return data[index]; };", False),
	("relocated-value-facts", "fn main() -> i32 { i32[2] data {}; usize index = 2; usize moved = $index; return data[moved]; };", True),
	("relocated-value-snapshot", "fn main() -> i32 { i32[2] data {}; usize index = 2; usize moved = $index; @index.usize(0); return data[moved]; };", True),
	("unknown-readonly-arithmetic", "fn multiply(usize value) -> usize { return value * value; }; fn inspect(usize value) -> i32 { i32[2] data {}; usize index = 2; multiply(value); return data[index]; };", True),
	("unknown-arithmetic-result", "fn multiply(usize value) -> usize { return value * value; }; fn inspect(usize value) -> i32 { i32[2] data {}; return data[multiply(value)]; };", False),
	("unknown-arithmetic-effects", "fn multiply(var usize& value) -> usize { return (@value = 0) * 2; }; fn main() -> i32 { i32[2] data {}; usize index = 2; multiply(@index); return data[index]; };", False),
	("throw-is-program-outcome", "fn checked(usize index) -> usize { if (index >= 2) { throw i32(7); }; return index; }; fn main() -> i32 { return i32(checked(3)); };", False),
	("normal-return-bounds", "fn checked(usize index) -> usize { if (index < 2) { throw i32(7); }; return index; }; fn inspect(usize index) -> i32 { i32[2] data {}; usize result = checked(index); return data[result]; };", True),
	("normal-return-wrapper", "fn checked(usize index) -> usize { if (index < 2) { throw i32(7); }; return index; }; fn wrapper(usize index) -> usize { return checked(index); }; fn inspect(usize index) -> i32 { i32[2] data {}; return data[wrapper(index)]; };", True),
	("normal-void-wrapper", "fn checked(usize index) { if (index < 2) { throw i32(7); }; }; fn wrapper(usize index) { checked(index); }; fn inspect(usize index) -> i32 { i32[2] data {}; wrapper(index); return data[index]; };", True),
	("caught-throw-effects", "fn reset(var usize& index) { @index = 0; throw i32(7); }; fn main() -> i32 { i32[2] data {}; usize index = 2; try { reset(@index); } catch (i32 caught) {}; return data[index]; };", False),
	("read-return-unknown", "fn checked(var usize* data, usize index) -> usize { if (index >= 2) { throw i32(7); }; return data[index]; }; fn inspect(var usize* data, usize index) -> i32 { i32[2] fixed {}; return fixed[checked(data, index)]; };", False),
	("identity-invalid", "fn identity(usize value) -> usize { return value; }; fn main() -> i32 { i32[2] values {}; return values[identity(2)]; };", True),
	("identity-valid", "fn identity(usize value) -> usize { return value; }; fn main() -> i32 { i32[2] values {}; return values[identity(1)]; };", False),
	("field-getter-invalid", "struct Counter { usize length; }; fn read(Counter& counter) -> usize { return counter.length; }; fn main() -> i32 { i32[2] values {}; Counter counter; @counter.length = 2; return values[read(counter)]; };", True),
	("field-mutator-invalid", "struct Counter { usize length; }; fn increase(var Counter& counter) { @counter.length = counter.length + 1; }; fn read(Counter& counter) -> usize { return counter.length; }; fn main() -> i32 { i32[2] values {}; Counter counter; @counter.length = 1; increase(@counter); return values[read(counter)]; };", True),
	("conditional-return-invalid", "fn select(usize value) -> usize { if (value < 2) { return 0; }; return 3; }; fn main() -> i32 { i32[2] values {}; return values[select(4)]; };", True),
	("conditional-return-valid", "fn select(usize value) -> usize { if (value < 2) { return 0; }; return 3; }; fn main() -> i32 { i32[2] values {}; return values[select(1)]; };", False),
	("guarded-write-invalid", "fn increase(var usize& value) -> bool { if (value < 3) { @value = value + 1; return true; }; return false; }; fn main() -> i32 { i32[2] values {}; usize value = 1; increase(@value); return values[value]; };", True),
	("opaque-pointer-write", "fn alter(var usize* value) { @*value = 0; }; fn main() -> i32 { i32[2] values {}; usize index = 2; alter(&@index); return values[index]; };", False),
	("multiple-return-paths", "fn inspect(usize index, usize length) -> i32 { if (index >= length) { return 1; }; if (index >= length) { return 2; }; return 0; };", False),
	("scalar-wrapper-invalid", "fn identity(usize value) -> usize { return value; }; fn wrap(usize value) -> usize { return identity(value); }; fn main() -> i32 { i32[2] values {}; return values[wrap(2)]; };", True),
	("reverse-wrapper-invalid", "fn wrap(usize value) -> usize { return identity(value); }; fn identity(usize value) -> usize { return value; }; fn main() -> i32 { i32[2] values {}; return values[wrap(2)]; };", True),
	("field-wrapper-invalid", "struct Counter { usize length; }; fn read(Counter& counter) -> usize { return counter.length; }; fn wrap(Counter& counter) -> usize { return read(counter); }; fn main() -> i32 { i32[2] values {}; Counter counter; @counter.length = 2; return values[wrap(counter)]; };", True),
	("recursive-wrapper-unknown", "fn recursive(usize value) -> usize { return recursive(value); }; fn inspect(usize value) -> i32 { i32[2] values {}; return values[recursive(value)]; };", False),
	("bool-result-state-invalid", "fn set(var usize& value, bool choice) -> bool { if (choice) { @value = 3; return true; }; return false; }; fn inspect(bool choice) -> i32 { i32[2] values {}; usize index = 1; bool result = set(@index, choice); if (result) { return values[index]; }; return 0; };", True),
	("bool-result-other-state-valid", "fn set(var usize& value, bool choice) -> bool { if (choice) { @value = 3; return true; }; return false; }; fn inspect(bool choice) -> i32 { i32[2] values {}; usize index = 1; bool result = set(@index, choice); if (!result) { return values[index]; }; return 0; };", False),
	("bool-result-state-merged", "fn set(var usize& value, bool choice) -> bool { if (choice) { @value = 3; return true; }; return false; }; fn inspect(bool choice) -> i32 { i32[2] values {}; usize index = 1; set(@index, choice); return values[index]; };", False),
	("bool-result-state-overwritten", "fn set(var usize& value, bool choice) -> bool { if (choice) { @value = 3; return true; }; return false; }; fn inspect(bool choice) -> i32 { i32[2] values {}; usize index = 1; bool result = set(@index, choice); @index = 0; if (result) { return values[index]; }; return 0; };", False),
	("union-result-state-invalid", "using Choice = i32 | i64; fn set(var usize& value, bool choice) -> Choice { if (choice) { @value = 3; return Choice(i32(0)); }; return Choice(i64(0)); }; fn inspect(bool choice) -> i32 { i32[2] values {}; usize index = 1; Choice result = set(@index, choice); if (result.has<i32>()) { return values[index]; }; return 0; };", True),
	("union-result-other-state-valid", "using Choice = i32 | i64; fn set(var usize& value, bool choice) -> Choice { if (choice) { @value = 3; return Choice(i32(0)); }; return Choice(i64(0)); }; fn inspect(bool choice) -> i32 { i32[2] values {}; usize index = 1; Choice result = set(@index, choice); if (result.has<i64>()) { return values[index]; }; return 0; };", False),
	("union-result-state-merged", "using Choice = i32 | i64; fn set(var usize& value, bool choice) -> Choice { if (choice) { @value = 3; return Choice(i32(0)); }; return Choice(i64(0)); }; fn inspect(bool choice) -> i32 { i32[2] values {}; usize index = 1; Choice result = set(@index, choice); return values[index]; };", False),
	("union-empty-record-state", "struct First {}; struct Other {}; using Choice = First | Other; fn set(var usize& value, bool choice) -> Choice { if (choice) { @value = 3; return Choice(First()); }; return Choice(Other()); }; fn inspect(bool choice) -> i32 { i32[2] values {}; usize index = 1; Choice result = set(@index, choice); if (result.has<First>()) { return values[index]; }; return 0; };", True),
	("union-payload-effects-unknown", "usize index; struct First { fn First(var First& this) { @index = 0; }; }; using Choice = First | i32; fn create() -> Choice { return Choice(First()); }; fn main() -> i32 { i32[2] values {}; @index = 2; Choice result = create(); return values[index]; };", False),
	("numeric-constructor-return", "fn identity(usize value) -> usize { return usize(value); }; fn main() -> i32 { i32[2] values {}; return values[identity(2)]; };", True),
	("deep-reverse-wrappers", " ".join(f"fn wrap{index}(usize value) -> usize {{ return wrap{index + 1}(value); }};" for index in range(8)) + " fn wrap8(usize value) -> usize { return value; }; fn main() -> i32 { i32[2] values {}; return values[wrap0(2)]; };", True),
	("virtual-target-unknown", "struct Base { fn index(Base& this) -> usize : [virtual] { return 2; }; }; struct Child : [Base] { fn index(Child& this) -> usize : [override] { return 0; }; }; fn inspect(Base& object) -> i32 { i32[2] values {}; return values[object.index()]; };", False),
	("switch-preserves-facts", "fn inspect(usize choice) -> i32 { i32[2] values {}; usize index = 2; switch (choice) { case 0: break; default: break; }; return values[index]; };", True),
	("switch-merge-invalid", "fn inspect(usize choice) -> i32 { i32[2] values {}; usize index = 0; switch (choice) { case 0: @index = 2; break; default: @index = 3; }; return values[index]; };", True),
	("switch-missing-default", "fn inspect(usize choice) -> i32 { i32[2] values {}; usize index = 1; switch (choice) { case 0: @index = 2; break; }; return values[index]; };", False),
	("switch-fallthrough", "fn main() -> i32 { i32[2] values {}; usize index = 2; switch (0) { case 0: @index = 3; case 1: @index = 0; break; default: @index = 3; }; return values[index]; };", False),
	("switch-constant-break", "fn main() -> i32 { i32[2] values {}; usize index = 0; switch (0) { case 0: @index = 2; break; default: @index = 0; }; return values[index]; };", True),
	("try-normal-preserves-facts", "fn main() -> i32 { i32[2] values {}; usize index = 2; try { @index = 3; } catch (i32 caught) { @index = 0; }; return values[index]; };", True),
	("try-catch-merge-invalid", "fn inspect(bool choice) -> i32 { i32[2] values {}; usize index = 0; try { if (choice) { throw i32(1); }; @index = 3; } catch (i32 caught) { @index = 3; }; return values[index]; };", True),
	("try-typed-handlers-conservative", "fn main() -> i32 { i32[2] values {}; usize index = 0; try { throw i32(1); } catch (i32 first) { @index = 3; } catch (i64 second) { @index = 0; }; return values[index]; };", False),
	("try-handler-rethrow-outer", "fn main() -> i32 { i32[2] values {}; usize index = 0; try { try { throw i32(1); } catch (i32 first) { throw; }; } catch (i32 second) { @index = 3; }; return values[index]; };", True),
	("scope-destructor-effects", "usize index; struct Change { fn ~Change(var Change& this) { @index = 0; }; }; fn main() -> i32 { i32[2] values {}; { Change change; @index = 2; }; return values[index]; };", False),
	("break-destructor-effects", "usize index; struct Change { fn ~Change(var Change& this) { @index = 0; }; }; fn main() -> i32 { i32[2] values {}; while (true) { Change change; @index = 2; break; }; return values[index]; };", False),
	("continue-destructor-effects", "usize index; struct Change { fn ~Change(var Change& this) { @index = 0; }; }; fn main() -> i32 { i32[2] values {}; usize count = 0; while (count < 1) { Change change; @index = 2; @count = count + 1; continue; }; return values[index]; };", False),
	("local-reference-alias-effects", "fn reset(var usize& value) { var usize& alias = @value; @alias = 0; }; fn main() -> i32 { i32[2] values {}; usize index = 2; reset(@index); return values[index]; };", False),
	("reference-field-alias-effects", "struct Holder { var usize& reference; }; fn reset(var Holder& holder) { @holder.reference = 0; }; fn main() -> i32 { i32[2] values {}; usize index = 2; Holder holder = Holder(@index); reset(@holder); return values[index]; };", False),
	("temporary-statement-effects", "usize index; struct Changer { fn ~Changer(var Changer& this) { @index = 0; }; }; fn pure(Changer& object, usize value) -> usize { return value; }; fn main() -> i32 { i32[2] values {}; pure(Changer(), (@index = 2)); return values[index]; };", False),
	("temporary-condition-effects", "usize index; struct Changer { fn ~Changer(var Changer& this) { @index = 0; }; }; fn pure(Changer& object, usize value) -> usize { return value; }; fn main() -> i32 { i32[2] values {}; if (pure(Changer(), (@index = 2))) {}; return values[index]; };", False),
	("temporary-loop-effects", "usize index; struct Changer { fn ~Changer(var Changer& this) { @index = 0; }; }; fn pure(Changer& object, usize value) -> usize { return value; }; fn main() -> i32 { i32[2] values {}; while (pure(Changer(), (@index = 2))) { break; }; return values[index]; };", False),
	("temporary-switch-effects", "usize index; struct Changer { fn ~Changer(var Changer& this) { @index = 0; }; }; fn pure(Changer& object, usize value) -> usize { return value; }; fn main() -> i32 { i32[2] values {}; switch (pure(Changer(), (@index = 2))) { case 2: break; default: break; }; return values[index]; };", False),
	("temporary-logical-effects", "usize index; struct Changer { fn ~Changer(var Changer& this) { @index = 0; }; }; fn pure(Changer& object, usize value) -> usize { return value; }; fn main() -> i32 { i32[2] values {}; if (pure(Changer(), (@index = 2)) && true) {}; return values[index]; };", False),
	("temporary-call-result-effects", "usize index; struct Changer { fn ~Changer(var Changer& this) { @index = 0; }; }; fn pure(Changer& object, usize value) -> usize { return value; }; fn create() -> Changer { return Changer(); }; fn main() -> i32 { i32[2] values {}; pure(create(), (@index = 2)); return values[index]; };", False),
]

AFFINE_SOURCE = """
fn grow(var usize& value) -> bool {
	if (value == usize::max) { return false; };
	@value = value + 1;
	return true;
};
fn shrink(var usize& value) -> bool {
	if (value == 0) { return false; };
	@value = value - 1;
	return true;
};
fn grow_wrapper(var usize& value) -> bool { return grow(@value); };
fn composed_verify(usize value) -> bool {
	usize old = value;
	bool changed = grow_wrapper(@value);
	if (changed) { return value > old; };
	return true;
};
fn verify(usize value) -> bool {
	usize old = value;
	bool changed = grow(@value);
	if (changed) { return value > old; };
	return true;
};
fn local(usize value) -> bool {
	if (value < 100) {
		usize grown = value + 1;
		return !(grown <= value);
	};
	return true;
};
fn uncertain(usize value) -> bool {
	usize grown = value + 1;
	return grown > value;
};
fn checked(usize index, usize length) -> bool {
	if (index >= length) { return false; };
	return true;
};
fn guard(usize index, usize length) -> bool {
	if (checked(index, length)) { return index < length; };
	return true;
};
fn loose(usize index, usize length, bool bypass) -> bool {
	if (bypass) { return true; };
	if (index >= length) { return false; };
	return true;
};
fn guard_loose(usize index, usize length, bool bypass) -> bool {
	if (loose(index, length, bypass)) { return index < length; };
	return true;
};
fn replace(var usize& value, usize replacement) { @value = replacement; };
fn guard_changed(usize index, usize length, usize replacement) -> bool {
	if (checked(index, length)) {
		replace(@index, replacement);
		return index < length;
	};
	return true;
};
struct Counter { usize length; };
fn read(Counter& counter) -> usize { return counter.length; };
struct Initialized { usize first = 1; usize second = 2; };
fn initialized_siblings(usize value) -> bool {
	Initialized fields;
	if (fields.first == fields.second) { return false; };
	@fields.first = value;
	return fields.second == 2;
};
fn default_getter() -> bool {
	Counter counter;
	if (read(counter) == 0) { return true; };
	return false;
};
fn getter_identity(usize value) -> bool {
	Counter counter;
	usize snapshot = value;
	@counter.length = snapshot;
	if (read(counter) == snapshot) { return true; };
	return false;
};
fn getter_growth(usize value) -> bool {
	Counter counter;
	usize snapshot = value;
	@counter.length = snapshot;
	if (grow(@counter.length)) {
		if (read(counter) > snapshot) { return true; };
		return false;
	};
	return true;
};
fn getter_shrink(usize value) -> bool {
	Counter counter;
	usize snapshot = value;
	@counter.length = snapshot;
	if (shrink(@counter.length)) {
		if (read(counter) < snapshot) { return true; };
		return false;
	};
	return true;
};
fn main() -> i32 {
	if (!verify(0) || !verify(usize::max - 1) || !verify(usize::max)) { return 1; };
	if (!composed_verify(0) || !composed_verify(usize::max - 1) || !composed_verify(usize::max)) { return 10; };
	if (!default_getter()) { return 11; };
	if (!initialized_siblings(0) || !initialized_siblings(2) || !initialized_siblings(usize::max)) { return 12; };
	if (!local(0) || !local(99) || !local(100)) { return 2; };
	if (uncertain(usize::max)) { return 3; };
	if (!guard(0, 2) || !guard(3, 2)) { return 4; };
	if (guard_loose(3, 2, true)) { return 5; };
	if (guard_changed(0, 2, 3)) { return 6; };
	if (!getter_identity(0) || !getter_identity(usize::max)) { return 7; };
	if (!getter_growth(0) || !getter_growth(usize::max)) { return 8; };
	if (!getter_shrink(0) || !getter_shrink(usize::max)) { return 9; };
	return 42;
};
"""


def check_affine_codegen(compiler, directory, function, expected):
	path = directory / "affine-growth.rt"
	path.write_text(AFFINE_SOURCE, encoding="utf-8")
	output = directory / "affine-growth.obj"
	subprocess.run([str(compiler), str(path), "--output", str(output),
		"--optimization", "0"], capture_output=True, text=True, check=True, timeout=30)
	toolsets = sorted(Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC").glob("*/bin/Hostx64/x64/dumpbin.exe"))
	if not toolsets:
		raise RuntimeError("dumpbin.exe unavailable for required code-generation verification")
	disassembly = subprocess.run([str(toolsets[-1]), "/nologo", "/disasm", str(output)],
		capture_output=True, text=True, check=True, timeout=30).stdout
	(directory / "affine-growth.disasm.txt").write_text(disassembly, encoding="utf-8")
	match = re.search(rf"(?m)^{re.escape(function)}:\s*\n", disassembly)
	if not match:
		raise RuntimeError(f"missing disassembly for {function}")
	body = re.split(r"(?m)^[^\s:]+:\s*$|^\s+Summary\s*$", disassembly[match.end():], maxsplit=1)[0]
	# LLVM may emit subtraction plus SETcc instead of CMP for a returned bool.
	comparisons = len(re.findall(r"\b(?:cmp|set[a-z]+)\s", body))
	retained = function in ("uncertain", "guard_loose", "guard_changed")
	if (retained and not comparisons or not retained and comparisons != expected):
		raise RuntimeError(f"expected {expected} comparisons in {function} at optimization 0, found {comparisons}\n{body}")
	if function == "composed_verify" and not re.search(r"\bcall\s+grow_wrapper\b", body):
		raise RuntimeError("composed proof check lost the required wrapper call at optimization 0")
	print(f"affine-{function}-codegen: PASS")


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", type=Path, default=Path("build/release/native/rutilec.exe"))
	options = parser.parse_args()
	compiler = (ROOT / options.compiler).resolve()
	directory = ROOT / "build/compiler-new/function-facts"
	directory.mkdir(parents=True, exist_ok=True)
	failures = []
	for name, source, rejected in CASES:
		path = directory / (name + ".rt")
		path.write_text(source + "\n", encoding="utf-8")
		try:
			result = subprocess.run([str(compiler), str(ROOT / "compiler/std/variant.rt"), str(path), "--output",
				str(directory / (name + ".obj")), "--optimization", "0"],
				capture_output=True, text=True, timeout=30)
			output = result.stdout + result.stderr
			if rejected:
				if result.returncode != 1 or BOUNDS not in output:
					raise RuntimeError(f"expected bounds diagnostic; exit {result.returncode}\n{output}")
			elif result.returncode:
				raise RuntimeError(f"unexpected exit {result.returncode}\n{output}")
			print(name + ": PASS")
		except (OSError, RuntimeError, subprocess.SubprocessError) as error:
			failures.append(name)
			print(f"{name}: FAIL: {error}")
	path = directory / "runtime-mutation.rt"
	path.write_text("fn increase(var usize& value) -> bool { if (value < 42) { "
		"@value = value + 1; return true; }; return false; }; "
		"fn main() -> i32 { usize value = 41; increase(@value); return i32(value); };\n",
		encoding="utf-8")
	try:
		run_case("facts-runtime-mutation", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	except (OSError, RuntimeError, subprocess.SubprocessError) as error:
		failures.append("runtime-mutation")
		print(f"runtime-mutation: FAIL: {error}")
	for function, expected in [("verify", 0), ("local", 1), ("uncertain", 1),
		("guard", 0), ("guard_loose", 1), ("guard_changed", 1),
		("getter_identity", 0), ("getter_growth", 0), ("getter_shrink", 0), ("composed_verify", 0), ("default_getter", 0), ("initialized_siblings", 0)]:
		try:
			check_affine_codegen(compiler, directory, function, expected)
		except (OSError, RuntimeError, subprocess.SubprocessError) as error:
			failures.append(f"affine-{function}-codegen")
			print(f"affine-{function}-codegen: FAIL: {error}")
	try:
		run_case("facts-affine-runtime", [(directory / "affine-growth.rt").relative_to(ROOT).as_posix()], compiler=str(compiler))
	except (OSError, RuntimeError, subprocess.SubprocessError) as error:
		failures.append("affine-runtime")
		print(f"affine-runtime: FAIL: {error}")
	path = directory / "cleanup-runtime.rt"
	path.write_text("""
usize index;
i32 destructions;
struct Change {
	fn ~Change(var Change& this) { @index = 0; @destructions = destructions + 1; };
};
fn before_return() -> usize { Change change; @index = 2; return index; };
fn pure(Change& object, usize value) -> usize { return value; };
fn with_temporary() -> usize { return pure(Change(), (@index = 2)); };
fn main() -> i32 {
	{ Change change; @index = 2; };
	if (index != 0 || destructions != 1) { return 1; };
	usize captured = before_return();
	if (captured != 2 || index != 0 || destructions != 2) { return 2; };
	while (true) { Change change; @index = 2; break; };
	if (index != 0 || destructions != 3) { return 3; };
	usize iterations = 0;
	while (iterations < 2) {
		Change change; @index = 2; @iterations = iterations + 1; continue;
	};
	if (index != 0 || destructions != 5) { return 4; };
	try { Change change; @index = 2; throw i32(7); }
	catch (i32 caught) { if (index != 0 || destructions != 6) { return 5; }; };
	usize snapshot = pure(Change(), (@index = 2));
	if (snapshot != 2 || index != 0 || destructions != 7) { return 6; };
	usize returned = with_temporary();
	if (returned != 2 || index != 0 || destructions != 8) { return 7; };
	return 42;
};
""", encoding="utf-8")
	try:
		run_case("facts-cleanup-runtime", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	except (OSError, RuntimeError, subprocess.SubprocessError) as error:
		failures.append("cleanup-runtime")
		print(f"cleanup-runtime: FAIL: {error}")
	path = directory / "float-unordered-runtime.rt"
	path.write_text("fn equal(f64 value) -> bool { return value == value; }; "
		"fn ordered(f64 value) -> bool { return value <= value; }; "
		"fn identity(f64 value) -> f64 { return value; }; "
		"fn wrapped(f64 value) -> bool { return identity(value) == identity(value); }; "
		"fn main() -> i32 { f64 zero = 0.0; f64 value = zero / zero; "
		"if (equal(value) || ordered(value) || wrapped(value)) { return 1; }; "
		"return 42; };\n", encoding="utf-8")
	try:
		run_case("facts-float-unordered-runtime", [path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	except (OSError, RuntimeError, subprocess.SubprocessError) as error:
		failures.append("float-unordered-runtime")
		print(f"float-unordered-runtime: FAIL: {error}")
	path = directory / "composed-runtime.rt"
	path.write_text("""
using Choice = i32 | i64;
fn select(var usize& n, bool choice) -> bool {
	if (choice) { @n = 3; return true; };
	@n = 1;
	return false;
};
fn bool_wrapper(var usize& n, bool choice) -> bool {
	bool result = select(@n, choice);
	return result;
};
fn tagged(var usize& n, bool choice) -> Choice {
	if (choice) { @n = 3; return Choice(i32(0)); };
	@n = 1;
	return Choice(i64(0));
};
fn union_wrapper(var usize& n, bool choice) -> Choice { return tagged(@n, choice); };
fn increment(var u08& n) -> bool { @n = n + 1; return true; };
fn narrow_wrapper(var u08& n) -> bool { return increment(@n); };
fn main() -> i32 {
	usize n = 0;
	if (!bool_wrapper(@n, true) || n != 3) { return 1; };
	if (bool_wrapper(@n, false) || n != 1) { return 2; };
	Choice first = union_wrapper(@n, true);
	if (!first.has<i32>() || n != 3) { return 3; };
	Choice second = union_wrapper(@n, false);
	if (!second.has<i64>() || n != 1) { return 4; };
	u08 small = 255;
	if (!narrow_wrapper(@small) || small != 0) { return 5; };
	return 42;
};
""", encoding="utf-8")
	try:
		run_case("facts-composed-runtime", ["compiler/std/variant.rt", path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	except (OSError, RuntimeError, subprocess.SubprocessError) as error:
		failures.append("composed-runtime")
		print(f"composed-runtime: FAIL: {error}")
	path = directory / "default-buffer-runtime.rt"
	path.write_text("fn emptyverify() -> i32 { rt::buffer<i32> buffer; "
		"if (buffer.size() != 0) { return 1; }; return 42; }; "
		"fn main() -> i32 { return emptyverify(); };\n", encoding="utf-8")
	try:
		output = directory / "default-buffer.obj"
		subprocess.run([str(compiler), str(ROOT / "compiler/std/buffer.rt"), str(path),
			"--output", str(output), "--optimization", "0"], capture_output=True,
			text=True, check=True, timeout=30)
		toolsets = sorted(Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC").glob("*/bin/Hostx64/x64/dumpbin.exe"))
		if not toolsets:
			raise RuntimeError("dumpbin.exe unavailable for required code-generation verification")
		disassembly = subprocess.run([str(toolsets[-1]), "/nologo", "/disasm", str(output)],
			capture_output=True, text=True, check=True, timeout=30).stdout
		(directory / "default-buffer.disasm.txt").write_text(disassembly, encoding="utf-8")
		match = re.search(r"(?m)^emptyverify:\s*\n", disassembly)
		if not match:
			raise RuntimeError("missing disassembly for emptyverify")
		body = re.split(r"(?m)^[^\s:]+:\s*$|^\s+Summary\s*$", disassembly[match.end():], maxsplit=1)[0]
		if re.search(r"\b(?:cmp|set[a-z]+)\s", body):
			raise RuntimeError("default buffer size comparison remains at optimization 0\n" + body)
		print("default-buffer-codegen: PASS")
		run_case("facts-default-buffer-runtime", ["compiler/std/buffer.rt", path.relative_to(ROOT).as_posix()], compiler=str(compiler))
	except (OSError, RuntimeError, subprocess.SubprocessError) as error:
		failures.append("default-buffer-runtime")
		print(f"default-buffer-runtime: FAIL: {error}")
	if failures:
		raise SystemExit(f"{len(failures)} function-facts case(s) failed: {', '.join(failures)}")


if __name__ == "__main__":
	main()
