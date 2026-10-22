"""Check inferred local reference loans and non-lexical last-use boundaries."""

import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
CASES = [
    ("shared", "i32 value = 4; const i32& a = value; const i32& b = value; return a + b;", True),
    ("exclusive-conflict", "i32 value = 4; var i32& a = @value; var i32& b = @value; return a + b;", False),
    ("shared-write", "i32 value = 4; const i32& a = value; @value = 5; return a;", False),
    ("last-use", "i32 value = 4; const i32& a = value; i32 copy = a; @value = 5; return copy;", True),
    ("exclusive-direct-read", "i32 value = 4; var i32& a = @value; i32 copy = value; return a + copy;", False),
    ("local-return", "i32 value = 4; return value;", False),
    ("reborrow", "i32 value = 4; var i32& a = @value; var i32& b = @a; @b = 5; return a;", True),
    ("reborrow-parent-write", "i32 value = 4; var i32& a = @value; const i32& b = a; @a = 5; return b;", False),
    ("exclusive-branches", "i32 value = 4; const i32& a = value; if (condition) { @value = 5; } else { return a; }; return value;", True),
    ("independent-fields", "Pair pair; var i32& a = @pair.first; var i32& b = @pair.second; return a + b;", True),
    ("loop-borrow", "i32 value = 4; const i32& a = value; while (condition) { @value = 5; }; return a;", False),
    ("mutable-second-argument", "i32 value = 4; const i32& a = value; mutate_second(0, @value); return a;", False),
    ("opaque-retains-loan", "i32 value = 4; const i32& a = value; retain(a); @value = 5; return value;", False),
    ("returned-shared", "i32 value = 4; const i32& a = shared_identity(value); @value = 5; return a;", False),
    ("returned-exclusive", "i32 value = 4; var i32& a = @identity(@value); i32 copy = value; return a + copy;", False),
    ("returned-wrapper", "i32 value = 4; const i32& a = wrapper(@value); @value = 5; return a;", False),
    ("local-return-wrapper", "i32 value = 4; return wrapper(@value);", False),
    ("ambiguous-first", "i32 first = 4; i32 second = 5; var i32& selected = @choose(condition, @first, @second); @first = 6; return selected;", False),
    ("ambiguous-second", "i32 first = 4; i32 second = 5; var i32& selected = @choose(condition, @first, @second); @second = 6; return selected;", False),
    ("ambiguous-reborrow", "i32 first = 4; i32 second = 5; var i32& selected = @choose(condition, @first, @second); var i32& child = @selected; @first = 6; return child;", False),
    ("stored-shared", "i32 value = 4; Holder holder = Holder(value); @value = 5; return holder.reference;", False),
    ("stored-last-use", "i32 value = 4; Holder holder = Holder(value); i32 copy = holder.reference; @value = 5; return copy;", True),
    ("stored-exclusive", "i32 value = 4; MutableHolder holder = MutableHolder(@value); i32 copy = value; return holder.reference + copy;", False),
    ("stored-copy", "i32 value = 4; Holder holder = Holder(value); Holder other = Holder(holder); @value = 5; return other.reference;", False),
    ("stored-move", "i32 value = 4; Holder holder = Holder(value); Holder other = Holder($holder); @value = 5; return other.reference;", False),
    ("stored-duplicate-exclusive", "i32 value = 4; DoubleMutable holder = DoubleMutable(@value, @value); return holder.first;", False),
    ("stored-local-return", "i32 value = 4; return Holder(value);", False),
    ("stored-local-move-return", "i32 value = 4; Holder holder = Holder(value); return $holder;", False),
    ("opaque-direct-owner", "i32 value = 4; retain(value); @value = 5; return value;", False),
    ("returned-aggregate", "i32 value = 4; Holder holder = make_holder(value); @value = 5; return holder.reference;", False),
    ("returned-aggregate-alias", "i32 value = 4; Holder holder = make_holder_alias(value); @value = 5; return holder.reference;", False),
    ("assigned-aggregate", "i32 first = 4; i32 second = 5; Holder holder = Holder(first); @holder = Holder(second); @second = 6; return holder.reference;", False),
    ("opaque-returned-reference", "i32 value = 4; retain(shared_identity(value)); @value = 5; return value;", False),
    ("nested-aggregate", "i32 value = 4; NestedHolder holder = NestedHolder(Holder(value)); @value = 5; return holder.inner.reference;", False),
    ("nested-last-use", "i32 value = 4; NestedHolder holder = NestedHolder(Holder(value)); i32 copy = holder.inner.reference; @value = 5; return copy;", True),
    ("nested-move", "i32 value = 4; NestedHolder first = NestedHolder(Holder(value)); NestedHolder holder = NestedHolder($first); @value = 5; return holder.inner.reference;", False),
    ("declaration-initializer", "DefaultHolder holder; @shared_value = 5; return holder.reference;", False),
    ("custom-declaration-initializer", "CustomDefault holder; @shared_value = 5; return holder.reference;", False),
    ("nested-declaration-initializer", "NestedDefault holder; @shared_value = 5; return holder.inner.reference;", False),
    ("array-disjoint", "i32[2] values; var i32& first = @values[0]; var i32& second = @values[1]; return first + second;", True),
    ("array-same", "i32[2] values; var i32& first = @values[0]; var i32& second = @values[0]; return first + second;", False),
    ("array-dynamic", "i32[2] values; usize index = unknown_index(); var i32& first = @values[0]; var i32& second = @values[index]; return first + second;", False),
    ("array-reborrow-disjoint", "i32[2] values; var i32& first = @values[0]; var i32& child = @first; var i32& second = @values[1]; return child + second;", True),
    ("virtual-call-opaque", "i32 value = 4; VirtualBorrow receiver; receiver.choose(@value); @value = 5; return value;", False),
    ("destructor-retains-reference", "i32 value = 4; ReadHolder holder = ReadHolder(value); var i32& exclusive = @value; return exclusive;", False),
    ("destructor-retains-write", "i32 value = 4; ReadHolder holder = ReadHolder(value); @value = 5; return value;", False),
    ("destructor-scope-end", "i32 value = 4; { ReadHolder holder = ReadHolder(value); }; @value = 5; return value;", True),
    ("explicit-destructor-ends-loan", "i32 value = 4; ReadHolder holder = ReadHolder(value); @holder.~ReadHolder(); @value = 5; return value;", True),
    ("store-inner-to-outer", "Holder outer = Holder(shared_value); { i32 local = 4; write_holder(@outer, local); }; return outer.reference;", False),
    ("store-wrapper-inner-to-outer", "Holder outer = Holder(shared_value); { i32 local = 4; write_wrapper(@outer, local); }; return outer.reference;", False),
    ("store-outer-to-inner", "i32 value = 4; { Holder inner = Holder(shared_value); write_holder(@inner, value); return inner.reference; };", True),
    ("store-global-escape", "i32 value = 4; write_holder(@global_holder, value); return 0;", False),
    ("store-overwritten-before-exit", "Holder outer = Holder(shared_value); { i32 local = 4; write_holder(@outer, local); write_holder(@outer, shared_value); }; return outer.reference;", True),
    ("store-conditional-overwrite", "Holder outer = Holder(shared_value); { i32 local = 4; write_holder(@outer, local); maybe_write(condition, @outer, shared_value); }; return outer.reference;", False),
    ("store-transitive-slot-copy", "Holder outer = Holder(shared_value); Holder copy = Holder(shared_value); { i32 local = 4; write_holder(@outer, local); write_holder(@copy, outer.reference); }; return copy.reference;", False),
    ("store-caught-throw-overwrite", "Holder outer = Holder(shared_value); { i32 local = 4; write_holder(@outer, local); try { throwing_write(condition, @outer, shared_value); } catch (i32 caught) {}; }; return outer.reference;", False),
    ("store-caught-helper-overwrite", "Holder outer = Holder(shared_value); { i32 local = 4; write_holder(@outer, local); caught_write(condition, @outer, shared_value); }; return outer.reference;", False),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True)
    args = parser.parse_args()
    directory = ROOT / "build/compiler-new/reference-loans"
    directory.mkdir(parents=True, exist_ok=True)
    compiler = Path(args.compiler).resolve()
    for name, body, valid in CASES:
        result_type = "const i32&" if name in ("local-return", "local-return-wrapper") else "i32"
        if name in ("stored-local-return", "stored-local-move-return"):
            result_type = "Holder"
        path = directory / (name + ".rt")
        source = (
            "struct Pair { i32 first; i32 second; };\n"
            "struct Holder { const i32& reference; };\n"
            "struct MutableHolder { var i32& reference; };\n"
            "struct DoubleMutable { var i32& first; var i32& second; };\n"
            "struct NestedHolder { Holder inner; };\n"
            "i32 shared_value = 4;\n"
            "Holder global_holder = Holder(shared_value);\n"
            "struct DefaultHolder { const i32& reference = shared_value; };\n"
            "struct CustomDefault { const i32& reference = shared_value; fn CustomDefault(var CustomDefault& this) {}; };\n"
            "struct NestedDefault { DefaultHolder inner; };\n"
            "struct VirtualBorrow { fn choose(const VirtualBorrow& this, var i32& value) -> var i32& : [virtual] { return @value; }; };\n"
            "i32 observed;\n"
            "struct ReadHolder { const i32& reference; fn ~ReadHolder(var ReadHolder& this) { @observed = this.reference; }; };\n"
            "fn mutate_second(i32 ignored, var i32& value) { @value = 5; };\n"
            "fn retain(const i32& value);\n"
            "fn unknown_index() -> usize;\n"
            "fn identity(var i32& value) -> var i32& { return @value; };\n"
            "fn shared_identity(const i32& value) -> const i32& { return value; };\n"
            "fn wrapper(var i32& value) -> var i32& { return @identity(@value); };\n"
            "fn choose(bool condition, var i32& first, var i32& second) -> var i32& { if (condition) { return @first; }; return @second; };\n"
            "fn make_holder(const i32& value) -> Holder { return Holder(value); };\n"
            "fn make_holder_alias(const i32& value) -> Holder { Holder holder = Holder(value); return $holder; };\n"
            "fn write_holder(var Holder& destination, const i32& value) { @destination = Holder(value); };\n"
            "fn write_wrapper(var Holder& destination, const i32& value) { write_holder(@destination, value); };\n"
            "fn maybe_write(bool condition, var Holder& destination, const i32& value) { if (condition) { write_holder(@destination, value); }; };\n"
            "fn throwing_write(bool condition, var Holder& destination, const i32& value) { if (condition) { throw i32(7); }; write_holder(@destination, value); };\n"
            "fn caught_write(bool condition, var Holder& destination, const i32& value) { try { if (condition) { throw i32(7); }; write_holder(@destination, value); } catch (i32 caught) {}; };\n"
            f"fn inspect(bool condition) -> {result_type} {{ {body} }};\n"
            "fn main() -> i32 { return 0; };\n"
        )
        path.write_text(source, encoding="utf-8")
        result = subprocess.run(
            [str(compiler), str(path), "--output", str(directory / (name + ".obj"))],
            capture_output=True, text=True, timeout=30,
        )
        output = result.stdout + result.stderr
        if valid:
            if result.returncode:
                raise RuntimeError(f"{name}: expected acceptance\n{output}")
        elif result.returncode != 1 or not any(message in output for message in (
            "access conflicts with a live reference loan",
            "returned reference refers to local storage",
            "reference members contain conflicting loans",
            "stored reference may outlive its referent",
        )):
            raise RuntimeError(f"{name}: expected loan diagnostic\n{output}")
        print(name + ": PASS")


if __name__ == "__main__":
    main()
