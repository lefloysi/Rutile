"""Check inferred local reference loans and non-lexical last-use boundaries."""

import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
CASES = [
    ("virtual-constructor-base-replacement", "CustomLayoutDerived object; object.value(); return 0;", False),
    ("virtual-wrapper-constructor-capture", "SafeDerived object; ctor_capture_dispatch(object); return 0;", False),
    ("virtual-wrapper-destructor-capture", "SafeDerived object; dtor_capture_dispatch(object); return 0;", False),
    ("virtual-nested-wrapper-destructor", "SafeDerived object; nested_dtor_dispatch(object); return 0;", False),
    ("virtual-base-replacement", "LayoutDerived object; var LayoutBase& alias = static_cast<var LayoutBase&>(@object); LayoutBase replacement; @alias = $replacement; object.value(); return 0;", False),
    ("virtual-before-base-replacement", "LayoutDerived object; object.value(); return 0;", True),
    ("virtual-helper-base-replacement", "LayoutDerived object; replace_layout(@object); object.value(); return 0;", False),
    ("virtual-wrapper-base-replacement", "LayoutDerived object; replace_then_dispatch(@object); return 0;", False),
    ("proven-unreachable-store", "Holder holder = Holder(shared_value); { i32 local = 4; bool known = false; if (known) { write_holder(@holder, local); }; }; return holder.reference;", True),
    ("unknown-conditional-local-return", "i32 value = 4; if (condition) { return value; }; return shared_value;", False),
    ("virtual-static-base-cast", "SafeDerived object; const SafeBase& alias = static_cast<const SafeBase&>(object); dispatch_safe(alias); return 0;", True),
    ("virtual-recursive-wrapper", "SafeDerived object; recursive_dispatch(condition, object); return 0;", False),
    ("virtual-capturing-target", "CaptureDerived object; dispatch_capture(object); return 0;", False),
    ("virtual-base-alias-known", "SafeDerived object; const SafeBase& alias = object; dispatch_safe(alias); return 0;", True),
    ("virtual-unknown-parameter", "i32 value = 4; condition.choose(@value); return 0;", False),
    ("virtual-raw-receiver", "SafeDerived object; change_receiver(&@object); dispatch_safe(object); return 0;", False),
    ("callback-stable-known", "SafeDerived object; var callback = &SafeBase::value; callback(object); return 0;", True),
    ("callback-replaced", "SafeDerived object; var callback = &SafeBase::value; @callback = &opaque_value; callback(object); return 0;", False),
    ("callback-helper-replaced", "SafeDerived object; var callback = &SafeBase::value; replace_callback(@callback); callback(object); return 0;", False),
    ("callback-alias-replaced", "SafeDerived object; var callback = &SafeBase::value; var alias = &@callback; @*alias = &opaque_value; callback(object); return 0;", False),
    ("proven-unreachable-write", "i32 value = 4; const i32& reference = value; bool known = false; if (known) { @value = 5; }; return reference;", True),
    ("proven-unreachable-local-return", "i32 value = 4; bool known = false; if (known) { return value; }; return shared_value;", True),
    ("proven-unreachable-raw", "Holder holder = Holder(shared_value); bool known = false; if (known) { overwrite_raw(&@holder); }; return holder.reference;", True),
    ("raw-helper-publication", "Holder holder = Holder(shared_value); publish_holder(&@holder); overwrite_escaped(); return holder.reference;", False),
    ("raw-wrapper-publication", "Holder holder = Holder(shared_value); publish_wrapper(&@holder); @holder = Holder(shared_value); overwrite_escaped(); return holder.reference;", False),
    ("cast-base-overlap", "BorrowDerived object; var BorrowBase& reference = static_cast<var BorrowBase&>(@object); const BorrowBase& view = object; return reference.value + view.value;", False),
    ("alias-reference-overlap", "BorrowAliases object; BorrowAliases::reference reference = @object.value; BorrowAliases::const_reference view = object.value; return reference + view;", False),
    ("raw-variadic-holder", "Holder holder = Holder(shared_value); overwrite_variadic(0, &@holder); return holder.reference;", False),
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
    ("virtual-known-noescape", "i32 value = 4; VirtualBorrow receiver; receiver.choose(@value); @value = 5; return value;", True),
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
    ("implicit-value-parameter-retained", "retain(condition); return 0;", False),
    ("implicit-scalar-retained", "i32 value = 4; retain(value); return 0;", False),
    ("implicit-array-retained", "i32[2] values; retain(values[0]); return 0;", False),
    ("implicit-field-retained", "Pair value; retain(value.first); return 0;", False),
    ("implicit-holder-retained", "i32 value = 4; { Holder holder = Holder(value); retain(holder.reference); }; return 0;", False),
    ("implicit-global-retained", "retain(shared_value); return 0;", True),
    ("implicit-ref-last-use", "i32 value = 4; { const i32& borrowed = value; i32 copy = borrowed; }; return value;", True),
    ("implicit-branch-retained", "if (condition) { i32 value = 4; retain(value); }; return 0;", False),
    ("implicit-loop-break-retained", "while (condition) { i32 value = 4; retain(value); break; }; return 0;", False),
    ("implicit-loop-continue-retained", "while (condition) { i32 value = 4; retain(value); continue; }; return 0;", False),
    ("implicit-throw-retained", "try { i32 value = 4; retain(value); throw i32(3); } catch (i32 caught) {}; return 0;", False),
    ("implicit-bypassed-declaration", "if (condition) { return 0; }; i32 value = 4; return value;", True),

    ("opaque-wrapper-end", "i32 value = 4; forward(value); return 0;", False),
    ("opaque-wrapper-deep", "i32 value = 4; forward_deep(value); return 0;", False),
    ("opaque-wrapper-alias", "i32 value = 4; forward_alias(value); return 0;", False),
    ("opaque-wrapper-recursive", "i32 value = 4; forward_recursive(condition, value); return 0;", False),
    ("opaque-wrapper-aggregate", "i32 value = 4; Holder holder = Holder(value); forward_holder(holder); return 0;", False),
    ("opaque-wrapper-field", "Pair value; forward_field(value); return 0;", False),
    ("pure-recursive-noescape", "i32 value = 4; pure_recursive(condition, value); return 0;", True),

    ("opaque-value-holder-move", "i32 value = 4; Holder holder = Holder(value); forward_value($holder); return 0;", False),
    ("opaque-value-holder-copy", "i32 value = 4; Holder holder = Holder(value); forward_value(Holder(holder)); return 0;", False),
    ("opaque-value-nested", "i32 value = 4; NestedHolder holder = NestedHolder(Holder(value)); forward_nested($holder); return 0;", False),
    ("pure-value-holder", "i32 value = 4; Holder holder = Holder(value); pure_value($holder); return 0;", True),

    ("deep-returned-reference", "i32 value = 4; const i32& borrowed = return_deep_20(value); return borrowed;", False),

    ("opaque-holder-object-end", "Holder holder = Holder(shared_value); retain_holder(holder); return 0;", False),
    ("content-wrapper-value", "i32 value = 4; Holder holder = Holder(value); forward_value_ref($holder); return 0;", False),
    ("content-wrapper-value-global", "Holder holder = Holder(shared_value); forward_value_ref($holder); return 0;", True),
    ("union-local-return", "i32 value = 4; return Choice(Holder(value));", False),
    ("union-stored-reference", "i32 value = 4; Choice choice = Choice(Holder(value)); @value = 5; return choice.get<Holder>().reference;", False),

    ("union-copy-reference", "i32 value = 4; Choice first = Choice(Holder(value)); Choice second = Choice(first); @value = 5; return second.get<Holder>().reference;", False),
    ("union-move-reference", "i32 value = 4; Choice first = Choice(Holder(value)); Choice second = Choice($first); @value = 5; return second.get<Holder>().reference;", False),
    ("union-extracted-reference", "i32 value = 4; Choice choice = Choice(Holder(value)); const Holder& payload = choice.get<Holder>(); @value = 5; return payload.reference;", False),

    ("destructor-captures-contents", "i32 value = 4; { RetainHolder holder = RetainHolder(value); }; return 0;", False),
    ("destructor-captures-global", "{ RetainHolder holder = RetainHolder(shared_value); }; return 0;", True),
    ("constructor-captures-this", "CaptureThis value; return 0;", False),

    ("nested-destructor-captures-contents", "i32 value = 4; { NestedRetainHolder holder = NestedRetainHolder(RetainHolder(value)); }; return 0;", False),
    ("nested-destructor-captures-global", "{ NestedRetainHolder holder = NestedRetainHolder(RetainHolder(shared_value)); }; return 0;", True),

    ("constructor-throws-after-capture", "try { ThrowCaptureThis value; } catch (i32 caught) {}; return 0;", False),

    ("opaque-special-constructor", "ForeignConstructed value; return 0;", False),
    ("opaque-special-destructor", "ForeignDestroyed value; return 0;", False),
    ("opaque-nested-constructor", "NestedForeignConstructed value; return 0;", False),

    ("unused-opaque-copy", "UnusedCopyOuter outer; return 0;", True),

    ("forward-declared-constructor", "ForwardOuter value; return 0;", True),
    ("forward-declared-capture", "i32 value = 4; forward_declared(value); return 0;", False),

    ("holder-array-source-write", "DefaultHolder[2] values; i32 value = 4; @values[0] = DefaultHolder(value); @value = 5; return values[0].reference;", False),
    ("holder-array-independent-element", "DefaultHolder[2] values; i32 value = 4; @values[0] = DefaultHolder(value); @value = 5; return values[1].reference;", True),
    ("holder-array-unknown-element", "DefaultHolder[2] values; i32 value = 4; @values[0] = DefaultHolder(value); @value = 5; return values[unknown_index()].reference;", False),
    ("holder-array-inner-store", "DefaultHolder[2] values; { i32 local = 4; @values[0] = DefaultHolder(local); }; return values[0].reference;", False),
    ("holder-array-overwrite", "DefaultHolder[2] values; { i32 local = 4; @values[0] = DefaultHolder(local); @values[0] = DefaultHolder(shared_value); }; return values[0].reference;", True),
    ("holder-array-sibling-overwrite", "DefaultHolder[2] values; { i32 local = 4; @values[0] = DefaultHolder(local); @values[1] = DefaultHolder(shared_value); }; return values[0].reference;", False),
    ("holder-array-unknown-overwrite", "DefaultHolder[2] values; { i32 local = 4; @values[0] = DefaultHolder(local); @values[unknown_index()] = DefaultHolder(shared_value); }; return values[0].reference;", False),
    ("raw-write-unrestricted", "i32 value = 4; const i32& shared = value; var i32* pointer = &@value; @*pointer = 5; return shared;", True),

    ("holder-array-copy", "DefaultHolder[2] values; i32 value = 4; @values[0] = DefaultHolder(value); DefaultHolder[2] copy = values; @value = 5; return copy[0].reference;", False),
    ("holder-array-move", "DefaultHolder[2] values; i32 value = 4; @values[0] = DefaultHolder(value); DefaultHolder[2] copy = $values; @value = 5; return copy[0].reference;", False),
    ("holder-array-copy-after-overwrite", "DefaultHolder[2] values; { i32 local = 4; @values[0] = DefaultHolder(local); @values[0] = DefaultHolder(shared_value); }; DefaultHolder[2] copy = values; return copy[0].reference;", True),
    ("holder-array-move-after-overwrite", "DefaultHolder[2] values; { i32 local = 4; @values[0] = DefaultHolder(local); @values[0] = DefaultHolder(shared_value); }; DefaultHolder[2] copy = $values; return copy[0].reference;", True),
    ("holder-array-reference-alias", "DefaultHolder[2] values; i32 value = 4; @values[0] = DefaultHolder(value); const i32& alias = values[0].reference; @value = 5; return alias;", False),
    ("holder-array-nested-field", "ArrayHolder holder; i32 value = 4; @holder.values[0] = DefaultHolder(value); @value = 5; return holder.values[0].reference;", False),
    ("holder-array-nested-independent", "ArrayHolder holder; i32 value = 4; @holder.values[0] = DefaultHolder(value); @value = 5; return holder.values[1].reference;", True),
    ("holder-array-local-return", "i32 value = 4; DefaultHolder[1] values; @values[0] = DefaultHolder(value); return $values;", False),
    ("holder-array-returned-store", "i32 value = 4; DefaultHolder[1] values = make_array(value); @value = 5; return values[0].reference;", False),
    ("holder-returned-store", "i32 value = 4; Holder holder = make_changed(value); @value = 5; return holder.reference;", False),
    ("raw-holder-opaque", "Holder holder = Holder(shared_value); overwrite_raw(&@holder); return holder.reference;", False),
    ("raw-holder-known", "Holder holder = Holder(shared_value); raw_replace(&@holder, shared_value); return holder.reference;", False),
    ("raw-holder-unused", "Holder holder = Holder(shared_value); overwrite_raw(&@holder); return 0;", True),
    ("raw-holder-restored", "Holder holder = Holder(shared_value); overwrite_raw(&@holder); @holder = Holder(shared_value); return holder.reference;", True),
    ("raw-holder-branch", "Holder holder = Holder(shared_value); if (condition) { overwrite_raw(&@holder); }; return holder.reference;", False),
    ("raw-holder-branch-restored", "Holder holder = Holder(shared_value); if (condition) { overwrite_raw(&@holder); }; @holder = Holder(shared_value); return holder.reference;", True),
    ("raw-holder-other-branch", "Holder holder = Holder(shared_value); if (condition) { overwrite_raw(&@holder); } else { @holder = Holder(shared_value); }; return holder.reference;", False),
    ("raw-array-restored-slot", "DefaultHolder[2] values; raw_default(&@values[0]); @values[0] = DefaultHolder(shared_value); return values[0].reference;", True),
    ("raw-array-unknown-slot", "DefaultHolder[2] values; raw_default(&@values[0]); @values[1] = DefaultHolder(shared_value); return values[0].reference;", False),
    ("raw-array-independent-slot", "DefaultHolder[2] values; raw_default(&@values[0]); return values[1].reference;", True),

    ("raw-placement-holder", "Holder holder = Holder(shared_value); i32 local = 4; var Holder* pointer = &@holder; @pointer->Holder(local); return holder.reference;", False),
    ("raw-placement-restored", "Holder holder = Holder(shared_value); i32 local = 4; var Holder* pointer = &@holder; @pointer->Holder(local); @holder = Holder(shared_value); return holder.reference;", True),
    ("raw-global-pointer-escape", "Holder holder = Holder(shared_value); @escaped_holder = &@holder; overwrite_escaped(); return holder.reference;", False),
    ("raw-retained-pointer-after-restore", "Holder holder = Holder(shared_value); overwrite_raw(&@holder); @holder = Holder(shared_value); overwrite_escaped(); return holder.reference;", False),

    ("returned-array-first-field", "i32 first = 4; i32 second = 5; DoubleDefault[1] values = make_pair_array(first, second); @first = 6; return values[0].first;", False),
    ("returned-array-second-field", "i32 first = 4; i32 second = 5; DoubleDefault[1] values = make_pair_array(first, second); @second = 6; return values[0].second;", False),

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
        if name == "holder-array-local-return":
            result_type = "DefaultHolder[1]"
        if name == "union-local-return":
            result_type = "Choice"
        parameter_type = "i32" if name == "implicit-value-parameter-retained" else "bool"
        path = directory / (name + ".rt")
        source = (
            "struct Pair { i32 first; i32 second; };\n"
            "fn retain_layout(const LayoutBase& object);\n"
            "struct LayoutBase { fn value(const LayoutBase& this) -> i32 : [virtual] { retain_layout(this); return 1; }; };\n"
            "struct LayoutDerived : [LayoutBase] { fn value(const LayoutDerived& this) -> i32 : [override] { return 0; }; };\n"
            "struct CustomLayoutDerived : [LayoutBase] { fn CustomLayoutDerived(var CustomLayoutDerived& this) { LayoutBase replacement; @static_cast<var LayoutBase&>(@this) = $replacement; }; fn value(const CustomLayoutDerived& this) -> i32 : [override] { return 0; }; };\n"
            "fn replace_layout(var LayoutBase& object) { LayoutBase replacement; @object = $replacement; };\n"
            "fn replace_then_dispatch(var LayoutBase& object) { replace_layout(@object); object.value(); };\n"
            "struct SafeBase { fn value(const SafeBase& this) -> i32 : [virtual] { return 0; }; };\n"
            "struct SafeDerived : [SafeBase] { fn value(const SafeDerived& this) -> i32 : [override] { return 1; }; };\n"
            "struct CaptureDerived : [SafeBase] { fn value(const CaptureDerived& this) -> i32 : [override] { retain_base(this); return 1; }; };\n"
            "fn retain_base(const SafeBase& object); fn change_receiver(var SafeDerived* object);\n"
            "fn opaque_value(const SafeBase& object) -> i32;\n"
            "using SafeCallback = ((const SafeBase& object) -> i32)*; fn replace_callback(var SafeCallback& callback) { @callback = &opaque_value; };\n"
            "fn dispatch_safe(const SafeBase& object) -> i32 { return object.value(); };\n"
            "struct ContextCtorProbe { fn ContextCtorProbe(var ContextCtorProbe& this, const SafeBase& object) { retain_base(object); }; };\n"
            "struct ContextDtorProbe { const SafeBase& object; fn ~ContextDtorProbe(var ContextDtorProbe& this) { retain_base(this.object); }; };\n"
            "fn ctor_capture_dispatch(const SafeBase& object) -> i32 { ContextCtorProbe probe = ContextCtorProbe(object); return object.value(); };\n"
            "fn dtor_capture_dispatch(const SafeBase& object) -> i32 { ContextDtorProbe probe = ContextDtorProbe(object); return object.value(); };\n"
            "fn nested_dtor_dispatch(const SafeBase& object) -> i32 { return dtor_capture_dispatch(object); };\n"
            "fn recursive_dispatch(bool condition, const SafeBase& object) -> i32 { if (condition) { return recursive_dispatch(condition, object); }; return object.value(); };\n"
            "fn dispatch_capture(const SafeBase& object) -> i32 { return object.value(); };\n"
            "struct BorrowBase { i32 value; }; struct BorrowDerived : [BorrowBase] {};\n"
            "struct BorrowAliases { i32 value; using reference = var i32&; using const_reference = const i32&; };\n"
            "fn overwrite_variadic(i32 tag, ...);\n"
            "struct Holder { const i32& reference; };\n"
            "struct MutableHolder { var i32& reference; };\n"
            "struct DoubleMutable { var i32& first; var i32& second; };\n"
            "struct NestedHolder { Holder inner; };\n"
            "i32 shared_value = 4;\n"
            "Holder global_holder = Holder(shared_value);\n"
            "struct DefaultHolder { const i32& reference = shared_value; };\n"
            "struct CustomDefault { const i32& reference = shared_value; fn CustomDefault(var CustomDefault& this) {}; };\n"
            "struct NestedDefault { DefaultHolder inner; };\n"
            "struct ArrayHolder { DefaultHolder[2] values; };\n"
            "struct DoubleDefault { const i32& first = shared_value; const i32& second = shared_value; };\n"
            "struct VirtualBorrow { fn choose(const VirtualBorrow& this, var i32& value) -> var i32& : [virtual] { return @value; }; };\n"
            "i32 observed;\n"
            "struct ReadHolder { const i32& reference; fn ~ReadHolder(var ReadHolder& this) { @observed = this.reference; }; };\n"
            "fn mutate_second(i32 ignored, var i32& value) { @value = 5; };\n"
            "fn retain(const i32& value);\n"
            "fn forward(const i32& value) { retain(value); };\n"
            "fn forward_deep(const i32& value) { forward(value); };\n"
            "fn forward_alias(const i32& value) { const i32& alias = value; retain(alias); };\n"
            "fn forward_recursive(bool condition, const i32& value) { if (condition) { forward_recursive(condition, value); } else { retain(value); }; };\n"
            "fn retain_holder(const Holder& value);\n"
            "fn forward_value_ref(Holder value) { forward_holder(value); };\n"
            "fn forward_value(Holder value) { retain(value.reference); };\n"
            "fn forward_nested(NestedHolder value) { retain(value.inner.reference); };\n"
            "fn pure_value(Holder value) {};\n"
            "fn forward_holder(const Holder& value) { retain(value.reference); };\n"
            "fn forward_field(const Pair& value) { retain(value.first); };\n"
            "fn pure_recursive(bool condition, const i32& value) { if (condition) { pure_recursive(condition, value); }; };\n"
            "struct RetainHolder { const i32& reference; fn ~RetainHolder(var RetainHolder& this) { retain(this.reference); }; };\n"
            "struct NestedRetainHolder { RetainHolder inner; };\n"
            "fn retain_this(const CaptureThis& value);\n"
            "struct CaptureThis { fn CaptureThis(var CaptureThis& this) { retain_this(this); }; };\n"
            "fn retain_throw_this(const ThrowCaptureThis& value);\n"
            "struct ThrowCaptureThis { fn ThrowCaptureThis(var ThrowCaptureThis& this) { retain_throw_this(this); throw i32(7); }; };\n"
            "struct ForeignConstructed { fn ForeignConstructed(var ForeignConstructed& this); };\n"
            "struct ForeignDestroyed { fn ~ForeignDestroyed(var ForeignDestroyed& this); };\n"
            "struct NestedForeignConstructed { ForeignConstructed member; };\n"
            "struct UnusedCopyPart { fn UnusedCopyPart(var UnusedCopyPart& this) : [default]; fn UnusedCopyPart(var UnusedCopyPart& this, const UnusedCopyPart& other); };\n"
            "struct UnusedCopyOuter { UnusedCopyPart part; };\n"
            "struct ForwardPart { fn ForwardPart(var ForwardPart& this); fn ForwardPart(var ForwardPart& this, const ForwardPart& other); };\n"
            "fn ForwardPart::ForwardPart(var ForwardPart& this) {};\n"
            "struct ForwardOuter { ForwardPart part; };\n"
            "fn forward_declared(const i32& value);\n"
            "fn forward_declared(const i32& value) { retain(value); };\n"
            "var Holder* escaped_holder;\n"
            "fn overwrite_escaped();\n"
            "fn publish_holder(var Holder* value) { @escaped_holder = value; };\n"
            "fn publish_wrapper(var Holder* value) { publish_holder(value); };\n"
            "fn overwrite_raw(var Holder* value);\n"
            "fn raw_default(var DefaultHolder* value);\n"
            "fn raw_replace(var Holder* destination, const i32& value) { @*destination = Holder(value); };\n"
            "fn make_changed(const i32& value) -> Holder { Holder holder = Holder(shared_value); write_holder(@holder, value); return $holder; };\n"
            "fn make_pair_array(const i32& first, const i32& second) -> DoubleDefault[1] { DoubleDefault[1] values; @values[0] = DoubleDefault(first, second); return $values; };\n"
            "fn make_array(const i32& value) -> DefaultHolder[1] { DefaultHolder[1] values; @values[0] = DefaultHolder(value); return $values; };\n"
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
            f"fn inspect({parameter_type} condition) -> {result_type} {{ {body} }};\n"
            "fn main() -> i32 { return 0; };\n"
        )
        if name.startswith("union-"):
            source = source.replace("struct Holder { const i32& reference; };", "struct Holder { const i32& reference; }; using Choice = Holder | i32;")
        if name == "deep-returned-reference":
            source += "fn return_deep_0(const i32& value) -> const i32& { return value; };\n"
            for level in range(1, 21):
                source += f"fn return_deep_{level}(const i32& value) -> const i32& {{ return return_deep_{level - 1}(value); }};\n"
        if name == "virtual-unknown-parameter":
            source = source.replace("fn inspect(bool condition)", "fn inspect(const VirtualBorrow& condition)")
        if name in ("proven-unreachable-local-return", "unknown-conditional-local-return"):
            source = source.replace("fn inspect(bool condition) -> i32", "fn inspect(bool condition) -> const i32&")
        path.write_text(source, encoding="utf-8")
        result = subprocess.run(
            [str(compiler), *([str(ROOT / "compiler/std/variant.rt")] if name.startswith("union-") else []), str(path), "--output", str(directory / (name + ".obj"))],
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
            "object lifetime ends while a dependent reference loan is active",
            "reference provenance analysis limit exceeded",
            "special member capture analysis limit exceeded",
            "constructor reference capture may outlive local storage",
            "implicit constructor capture provenance is unavailable",
            "reference holding projection exceeds the analysis depth limit",
            "reference contents have unknown provenance after a raw or opaque memory effect",
            "recursive returned reference contents require conservative rejection",
        )):
            raise RuntimeError(f"{name}: expected loan diagnostic\n{output}")
        print(name + ": PASS")


if __name__ == "__main__":
    main()
