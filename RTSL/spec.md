# RTSL Specification Index

The RTSL specification is split into focused documents under `docs/`.

- [Language semantics](docs/language.md): source syntax, declarations, name
  lookup, types, functions, uniforms, varyings, entries, and shader semantics.
- [Compiler architecture](docs/compiler-architecture.md): frontend pipeline,
  ownership boundaries, diagnostics, CLI responsibilities, and C ABI shape.
- [Artifact formats](docs/artifacts.md): binary container layout for `rtslo`,
  `rtsll`, `rtslm`, and `rtslp`.
- [RTIR](docs/rtir.md): typed backend-neutral IR, binary instruction records,
  textual assembly/disassembly, primitives, and debug mapping.
- [Linking](docs/linking.md): object linking, library linking, module/interface
  resolution, symbol identity, type resolution, and stale interface validation.
- [Backend contract](docs/backend-contract.md): what Rutile backends may assume
  when consuming linked `rtslp` artifacts.

The canonical keyword list is maintained in
[language semantics](docs/language.md#keywords).
