# RTSL Linking

The RTSL linker is shader-specific. It resolves symbols, types, calls,
resources, stage interfaces, and debug metadata. It does not perform CPU-style
address relocation or emit hardware machine code.

## Inputs And Outputs

Object linking consumes `rtslo` files and their referenced `rtslm` interfaces.

Library linking consumes `rtslo` and `rtsll` files and emits `rtsll`. If the
linked library exports symbols, the linker also emits `rtslm`.

Program linking consumes `rtslo` and `rtsll` files and emits `rtslp`.

Backends consume `rtslp` only.

## Module Interface Resolution

Imports name source-like paths:

```rtsl
import <lighting/brdf.rtsl>;
```

The compiler resolves that import to an interface artifact:

```text
lighting/brdf.rtsl -> lighting/brdf.rtslm
```

Module search paths preserve relative layout. If `-I build/rtsl` is provided,
the compiler searches `build/rtsl/lighting/brdf.rtslm`.

Importers consume `rtslm` data and do not reparse imported source files.

## Symbol Identity

The linker resolves functions and overloads by canonical identity:

```text
qualified_name + parameter_types + return_type
```

Records also preserve display names. Display names are used for diagnostics,
debugging, reflection, and disassembly. Canonical identities are used for
linking.

Symbol linkage states:

- local: visible only inside one implementation artifact
- exported: emitted through an `rtslm` interface
- imported: required from another object or library
- primitive: reserved backend-lowered operation
- entry: executable shader entry point

## Type Resolution

Types are resolved by structural and nominal rules defined by the language
semantics. Named exported types must match imported interface identities. Private
types may be merged only when doing so is ABI-safe and does not change debug
identity.

The linker validates type compatibility for function calls, exported symbols,
resource metadata, and stage interfaces.

## Object Linking

For `rtslo` inputs, the linker:

- merges string, type, symbol, constant, function, and debug tables
- resolves imported symbols against exports from other inputs or libraries
- rewrites resolved call references from symbol ids to function ids
- preserves unresolved external references only when producing `rtsll`
- reports unresolved non-primitive references when producing `rtslp`

## Library Linking

An `rtsll` is a linked implementation package without a required entry point.
It may contain exported symbols and may be used as an input to future links.

If a library exports symbols, a matching `rtslm` is emitted. The `rtslm` contains
the public interface only; the `rtsll` contains implementation bodies.

## Program Linking

An `rtslp` is the final backend-facing program artifact. Program linking
requires all non-primitive references to be resolved and at least one valid entry
point unless the caller explicitly requests a validation-only link mode.

The program linker finalizes:

- function ids
- entry table
- resource layout metadata
- stage-interface metadata
- debug/source maps
- final symbol and type tables

## Stale Interface Validation

Each `rtslm` stores a full interface hash and per-export hashes. Importers record
the hashes they compiled against. The compiler or linker rejects stale inputs
when a required interface hash no longer matches.

Interface hashes are based on semantic interface data, not on private bodies or
source formatting.

## Error Handling

Linking errors include unresolved symbols, duplicate exported canonical
identities, incompatible type identities, stale module interfaces, invalid entry
signatures, conflicting resource layouts, and unsupported primitive references.

Diagnostics should reference both the importing use site and the exporting or
conflicting definition when available.
