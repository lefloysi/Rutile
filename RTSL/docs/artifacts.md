# RTSL Artifact Formats

RTSL artifacts are binary files using one sectioned container family. Textual
RTIR exists for disassembly, assembly, tests, and inspection, but the canonical
toolchain interchange format is binary.

For v0.1, the artifact family is graphics-only: the compiler and linker support
the object, module/interface, library, and program forms needed by the current
vertex/fragment pipeline.

## Artifact Kinds

`rtslo` is a compiled object file for one source file. It contains implementation
bodies, local symbols, imports, exports, unresolved references, and debug data.

`rtsll` is a linked library file. It contains linked implementation bodies and
resolved internal references, but does not require executable entry points.

`rtslm` is a module/interface file. It contains exported interface data for an
object or library. It never contains private function bodies.

`rtslp` is a linked program file. It contains the final linked RTIR, resolved
entry points, resource metadata, stage metadata, and debug data consumed by
Rutile backends.

## Container Header

Every artifact starts with a fixed-size header:

```text
magic              u32  artifact magic
version_major      u16
version_minor      u16
artifact_kind      u16  rtslo, rtsll, rtslm, or rtslp
endianness         u8
flags              u32
header_size        u32
section_count      u32
section_table_off  u64
file_size          u64
content_hash       u128
```

The current implementation uses the same layout but still treats the format as
release-0.1 data. Future incompatible changes must bump the version fields and
be called out in release notes.

All offsets are from the start of the file. The initial format uses little
endian encoding. Unknown required flags make the artifact unreadable. Unknown
optional flags may be ignored by older tools.

## Section Table

Each section table entry contains:

```text
section_kind  u32
flags         u32
offset        u64
size          u64
alignment     u32
hash          u128
```

Sections are individually hashable so tools can validate stale interfaces,
incremental builds, and cache entries.

## Common Sections

`string_table` stores all display names, qualified names, source paths, import
paths, debug strings, and disassembly names.

`type_table` stores scalar, vector, matrix, struct, resource, generic, function,
and stage payload types.

`symbol_table` stores all symbols referenced by the artifact. Symbol records
include kind, linkage, display name, qualified name, canonical identity,
signature, type, source origin, and target table reference when present.

`constant_table` stores typed constants used by RTIR and metadata.

`debug_table` stores source file records, source spans, symbol origins,
function-origin records, and instruction-origin records.

`source_map` maps generated RTIR ranges back to original source spans.

## Implementation Sections

`function_table` stores function records and points into the instruction stream.

`block_table` stores basic block ranges and control-flow metadata.

`instruction_stream` stores binary RTIR instructions.

`reloc_table` stores unresolved symbolic references in `rtslo` and references
that still need final resolution when producing `rtslp`.

`import_table` records imported module paths, imported symbols, and the required
interface hashes.

`export_table` records symbols exported by this implementation artifact and the
matching interface records that must appear in `rtslm`.

`entry_table` records executable entry points and is required for `rtslp`.

`resource_table` records resource declarations, access qualifiers, resource
types, binding scopes, and reflected names.

`stage_interface_table` records varyings, interpolation qualifiers, stage input
and output payloads, and stage-family metadata.

## Interface Sections

`interface_export_table` records public exported symbols.

`interface_type_table` records public type signatures needed by importers.

`interface_resource_table` records public resource contracts when resources are
exportable or required by exported entry signatures.

`dependency_table` records imported interfaces and hashes used to validate stale
build products.

`interface_hash_table` records stable hashes for the full interface and for
individual exported symbols.

## Artifact Section Rules

`rtslo` requires header, string table, type table, symbol table, function table,
instruction stream, import table, and debug table. It includes export and
resource sections when needed. Calls to unresolved external symbols reference
symbol ids.

`rtsll` requires header, string table, type table, symbol table, function table,
instruction stream, debug table, and resource metadata. Internal references are
resolved. Export sections are present when the library has public symbols.

`rtslm` requires header, string table, public symbol/interface export table,
interface type table, dependency table, and interface hashes. It must not contain
private function bodies or private instruction streams.

`rtslp` requires header, string table, final type table, final symbol table,
function table, instruction stream, entry table, resource table,
stage-interface table, and debug table. Calls are resolved to final function ids
unless they target reserved primitives.

## Naming And Debug Preservation

Artifacts store both canonical identities and display names. Canonical identities
drive linking. Display names drive diagnostics, reflection, disassembly, and
debugging.

The linker must preserve authored names where possible. Generated temporaries may
use deterministic compiler names, but user-authored symbols, resources, entries,
types, fields, and namespaces keep their original display names.
