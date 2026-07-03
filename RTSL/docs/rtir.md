# RTSL Intermediate Representation (RTIR)

RTIR is a typed, SSA, instruction-stream IR. It is intentionally shaped like
SPIR-V so the Vulkan backend is mostly id remapping and opcode encoding rather
than a translation pass.

The old `BodyOp` model (statement strings carried through the artifact) is
removed. Lowering happens in the compiler frontend; backends consume only the
SSA stream and the metadata tables described in
[artifacts.md](artifacts.md).

## Model

RTIR is a flat instruction stream built around three primitives:

- **Ids** (`IRId`, 32-bit) name SSA values, types, constants, and functions.
  Ids are unique within a module. Id `0` is reserved as "no id".
- **Instructions** (`IRInstruction`) are tagged opcodes that produce zero or
  one result. They reference ids for operand values and types, and may carry
  inline literals (constant bit patterns, member indices, storage class, etc.).
- **Decorations** (`IRDecoration`) attach metadata to an existing id without
  changing the SSA stream. Locations, descriptor set/binding, builtin slot,
  and interpolation qualifiers are all decorations.

A module's id space is shared across types, constants, global variables, and
SSA function values. This matches SPIR-V exactly: a `%float` type id, a
`%const_zero` constant id, and a `%5` arithmetic result id all live in the
same numbering.

## Type model

The type system is parametric, not a fixed table of named types. Scalars carry
their width as a literal (`TypeInt 16`, `TypeFloat 32`, `TypeFloat 64`), and
aggregates are built up from those: `TypeVector(TypeFloat 32, 3)` is what
shader languages spell `vec3`, `TypeMatrix(TypeVector(TypeFloat 32, 4), 4)`
is `mat4`, and so on.

Two consequences:

- The IR can carry types the source language does not currently expose
  (8/16/64-bit integers, packed vectors, runtime arrays). The frontend just
  has to emit the right `TypeInt {width}` / `TypeVector` / `TypeArray`
  combination; the IR never had a `vec3` keyword to lose.
- Backends recognize common shapes and emit the natural target spelling:
  `TypeVector(TypeFloat 32, 3)` → GLSL `vec3` / SPIR-V `OpTypeVector %float 3`.
  When the target cannot represent a shape (e.g. GLSL without
  `GL_NV_gpu_shader5` cannot emit a 16-bit int), the backend either lifts to
  the nearest supported width or rejects the module with a diagnostic.

This is why the `type_constant_pool` is forward-only and deduplicated: the
backend walks it once, lazily computing the target spelling for each id, and
every later reference to that id (in operands, in struct members, in pointer
pointees) hits the same cached spelling. `TypeInt` and `TypeFloat` always
carry a `width` literal — there is no implicit "default int".

## Module layout

```text
IRModule
  source_name            : StringId
  strings                : StringTable
  type_constant_pool     : vector<IRInstruction>   // OpType*, OpConstant*
  global_variables       : vector<IRInstruction>   // OpVariable in non-Function storage classes
  decorations            : vector<IRDecoration>
  functions              : vector<IRFunction>

  uniforms               : vector<UniformBinding>  // reflection bridge
  stage_interfaces       : vector<StageInterface>  // reflection bridge
  structs                : vector<StructDecl>      // reflection bridge

  next_id                : IRId
```

`type_constant_pool` is ordered, deduplicated, and forward-only: a type id
may only reference ids that appear earlier in the pool. The same rule applies
to constants. This is the standard SPIR-V module ordering.

The reflection bridges are not part of the SSA semantics; they let
`rtslModuleGetUniform`, `rtslModuleGetStageVariable`, etc. answer queries
without re-deriving the data from decorations. They are written into the
`resource_table` and `stage_interface_table` artifact sections.

## Reflection contract for rtslp

Every `rtslp` is required to carry enough reflection to drive a backend's
pipeline-layout, descriptor-set, and vertex-input setup without re-parsing
the SSA stream. A consumer (Rutile's Vulkan backend, a tool, an editor) opens
an `rtslp`, walks the reflection tables, and can build pipelines from them
directly.

Required reflection content per `rtslp`:

- **`resource_table`** — one record per declared uniform / resource. Carries
  the enclosing uniform scope name, the source-level name, the resolved
  descriptor `set` and `binding`, an access qualifier (`read_write` /
  `read_only` / `write_only` as u8), and a `type_id` into the IR type pool.
  The type pool entry (plus the decoration table's `Offset` / `ArrayStride`
  / `MatrixStride` entries on that struct) describes the binding's full byte
  layout; opaqueness (sampler / image vs value uniform) is read off the
  type pool node's opcode rather than carried as a separate flag.
- **`stage_interface_table`** — one record per `input` / `varying` / `output`
  field across every payload type. Carries role, interpolation (u8 enum),
  builtin slot (u8 enum; `none` for user-located fields), and `location`
  (`0xffffffff` means "no location"; the builtin drives placement instead).
  Payload type name and field name are present only for `input` (host-visible
  vertex layout); varying and output records carry empty strings since the
  linker matches those by location.
- **`entry_table`** — one record per backend entry point. Carries the
  4-letter stage entry name (`vert`, `frag`), the canonical mangled
  symbol id, the SSA function id, the stage family, the parameter and return
  payload type ids, and the list of resource and stage-interface ids the
  entry transitively references.
- **`symbol_table`** — display name + mangled name + linkage for every named
  symbol (functions, exported types, resources). Reflection queries use
  display names; the SSA stream uses mangled identity.

These tables are *resolved*: descriptor sets, bindings, and locations are
fully assigned by the linker before being written into the `rtslp`. A
backend never has to invent a binding number. The decoration table in the
SSA stream mirrors the same assignments — they must agree.

The C ABI surfaces this directly:

- `rtslModuleGetUniformCount` / `rtslModuleGetUniform` reads `resource_table`.
- `rtslModuleGetStageVariableCount` / `rtslModuleGetStageVariable` /
  `rtslModuleGetStageLocation` reads `stage_interface_table`.
- `rtslModuleGetEntryCount` / `rtslModuleGetEntry` reads `entry_table`.

A backend that wants to ignore the SSA stream entirely (an out-of-tree
reflector, a pipeline-cache key generator) can do its whole job against these
tables.

## Functions and basic blocks

```text
IRFunction
  result_id          : IRId          // OpFunction result
  function_type_id   : IRId          // OpTypeFunction
  return_type_id     : IRId
  parameter_ids      : vector<IRId>  // each is the result of an OpFunctionParameter
  body               : vector<IRInstruction>
  stage              : StageKind
  generated          : bool
  display_name       : StringId
  mangled_name       : StringId
```

`body` is a flat list of instructions. Basic blocks are delimited by `Label`
instructions; every block ends with a terminator (`Branch`,
`BranchConditional`, `Return`, `ReturnValue`). Local `Variable` instructions
must appear in the first block of the function.

Structured control flow is described by `SelectionMerge` / `LoopMerge`
instructions placed just before a `BranchConditional`. The SPIR-V validation
rules apply unchanged.

## Instruction shape

```cpp
struct IRInstruction {
    IROp     op;           // opcode
    IRId     result_id;    // 0 if no result
    IRId     type_id;      // 0 if not typed (control flow, store, decoration target)
    SmallVec<IRId, 4> operands;
    SmallVec<u32, 2>  literals;
    DebugLocation loc;     // optional (file_id, line, column)
};
```

`operands` are always ids. Literals carry inline data: constant bit patterns,
member indices for `CompositeExtract`/`AccessChain`, storage class for
`Variable`, the index for `ReadInput`/`WriteOutput`, the builtin enum for
`ReadBuiltin`/`WriteBuiltin`, the ext-instruction number for `ExtInst`, etc.

## Opcode set (initial)

The starter set covers what the workspace shaders need today. The opcode
enum is open — new ops can be added as the language grows.

**Types** (results go into `type_constant_pool`):
`TypeVoid`, `TypeBool`, `TypeInt`, `TypeUInt`, `TypeFloat`,
`TypeVector` (operand: scalar type id; literal: component count),
`TypeMatrix` (operand: vector type id; literal: column count),
`TypeStruct` (operands: member type ids),
`TypePointer` (operand: pointee type id; literal: storage class),
`TypeArray` (operand: element type id; literal: length),
`TypeFunction` (operands: return type id, parameter type ids),
`TypeImage`, `TypeSampler`, `TypeSampledImage`.

**Constants**:
`ConstantBool`, `ConstantInt`, `ConstantUInt`, `ConstantFloat`,
`ConstantComposite` (operands: element ids).

**Variables and memory**:
`Variable` (literal: storage class), `Load`, `Store`, `AccessChain`
(operands: base id, index ids; index ids are constants for static structs and
arbitrary ids for runtime arrays).

**Composite**:
`CompositeConstruct`, `CompositeExtract` (literals: member indices),
`CompositeInsert`, `VectorShuffle` (literals: component indices).

**Arithmetic**:
`FAdd`, `FSub`, `FMul`, `FDiv`, `FMod`, `FNegate`,
`IAdd`, `ISub`, `IMul`, `SDiv`, `UDiv`, `SMod`, `UMod`.

**Vector / matrix**:
`VectorTimesScalar`, `MatrixTimesScalar`, `MatrixTimesVector`,
`MatrixTimesMatrix`, `Dot`, `Cross`.

**Comparison and logical**:
`FOrdEqual`, `FOrdNotEqual`, `FOrdLess`, `FOrdLessEqual`, `FOrdGreater`,
`FOrdGreaterEqual`, `IEqual`, `INotEqual`, `SLess`, `SLessEqual`, `SGreater`,
`SGreaterEqual`, `LogicalAnd`, `LogicalOr`, `LogicalNot`.

**Conversion**:
`ConvertFToU`, `ConvertFToS`, `ConvertSToF`, `ConvertUToF`, `Bitcast`.

**Control flow**:
`Label`, `Branch`, `BranchConditional`, `SelectionMerge`, `LoopMerge`,
`Return`, `ReturnValue`.

**Function**:
`FunctionParameter`, `FunctionCall`.

**Image**:
`SampledImage`, `ImageSampleImplicitLod`, `ImageSampleExplicitLod`,
`ImageRead`, `ImageWrite`.

**Stage I/O** (high-level RTSL primitives that backends lower to the
appropriate target form — for SPIR-V they become `OpLoad`/`OpStore` against
`Input`/`Output` storage class variables):
`ReadInput` (literal: location), `WriteOutput` (literal: location),
`ReadBuiltin` (literal: builtin slot), `WriteBuiltin` (literal: builtin slot).

**Extended instructions** (standard library calls that map to backend
extended sets — `sin`, `cos`, `normalize`, `length`, `mix`, ...):
`ExtInst` (literal: ext-instruction number from `rt.std.450`, an RTSL set
that aliases SPIR-V's `GLSL.std.450` where possible).

## Decorations

```cpp
enum class IRDecorationKind : u16 {
    Location, Binding, DescriptorSet,
    Offset, ArrayStride, MatrixStride,
    BuiltIn,
    NoPerspective, Flat, Centroid, Sample,
    NonWritable, NonReadable,
};

struct IRDecoration {
    IRId target;          // result_id being decorated (variable, struct member, etc.)
    IRDecorationKind kind;
    u32 member_index;     // u32(-1) for "not a member"
    SmallVec<u32, 2> literals;
};
```

Decorations are written into a dedicated artifact section (`decoration_table`)
so the linker can merge them and the backend can iterate them once.

## Storage classes

Subset used initially:
`Function`, `Input`, `Output`, `Uniform`, `UniformConstant`, `StorageBuffer`,
`PushConstant`, `Private`. Encoded as a `u8` in `Variable`'s first literal.

## Debug

`DebugLocation` (file_id + line + column) is carried per instruction. The
`debug_table` artifact section stores the file id → source path mapping. The
backend may emit `OpLine`/`OpNoLine` directly from these locations.

Authored function and symbol names are kept in `function.display_name` and
`function.mangled_name` (as `StringId`s into the artifact string table). The
linker preserves user-authored names; backends use the mangled name for stable
external identity and the display name for diagnostics.

## Example

For the workspace's `position = mvp * vec4(p.position, 1.0);` line, the
generated IR looks roughly like:

```text
%mat4    = OpTypeMatrix %vec4 4
%vec3    = OpTypeVector %float 3
%vec4    = OpTypeVector %float 4
%mvp_ptr = OpTypePointer Uniform %mat4
%mvp     = OpVariable %mvp_ptr Uniform
%one     = OpConstantFloat %float 1.0

%10 = OpLoad %mat4 %mvp
%11 = OpAccessChain %vec3 %p 0           ; p.position
%12 = OpLoad %vec3 %11
%13 = OpCompositeExtract %float %12 0
%14 = OpCompositeExtract %float %12 1
%15 = OpCompositeExtract %float %12 2
%16 = OpCompositeConstruct %vec4 %13 %14 %15 %one
%17 = OpMatrixTimesVector %vec4 %10 %16
%18 = OpAccessChain %vec4_ptr_out %this 0  ; this.position
       OpStore %18 %17
```

This is roughly what we'd write to a `.spv` file. The SPIR-V backend rewrites
ids to SPIR-V's `Id` numbering, maps opcodes, packs decorations into
`OpDecorate`, and emits the binary header. No lowering pass is required.

## Artifact wire format

The instruction stream is serialized as packed records:

```text
record:
  op           : u16
  result_id    : u32
  type_id      : u32
  operand_count: u16
  literal_count: u16
  operands     : u32 * operand_count
  literals     : u32 * literal_count
  loc          : u32 file_id + u32 (line<<12 | column)
```

Functions point into the stream by `[begin, end)` byte offset, as today. The
existing `string_table`, `symbol_table`, `entry_table`, `resource_table`,
`stage_interface_table`, `debug_table` sections remain — they describe the
module surface and reflection metadata. The new sections are:

- `type_table` repurposed to hold the type/constant pool instruction range
- `global_variable_table` for module-scope `Variable` instructions
- `decoration_table` for `IRDecoration` records

## Textual disassembly

`rtslc --dump-rtir` prints SPIR-V-style assembly:

```text
; rtslp 1.0  graphics.rtsl

%void   = OpTypeVoid
%float  = OpTypeFloat 32
%vec3   = OpTypeVector %float 3
%vec4   = OpTypeVector %float 4
%mat4   = OpTypeMatrix %vec4 4

OpDecorate %mvp DescriptorSet 0
OpDecorate %mvp Binding 0

%mvp_ptr = OpTypePointer Uniform %mat4
%mvp     = OpVariable %mvp_ptr Uniform

; fn vert_main(...)
%vert_main = OpFunction %Vertex None %vert_main_ty
%p         = OpFunctionParameter %Point
%entry     = OpLabel
%10        = OpLoad %mat4 %mvp
%11        = OpAccessChain %vec3_ptr_fn %p 0
%12        = OpLoad %vec3 %11
%13        = OpCompositeConstruct %vec4 %12 %one
%14        = OpMatrixTimesVector %vec4 %10 %13
             OpReturnValue %14
             OpFunctionEnd
```

The assembler (`rtslc --assemble-rtir`) is optional for the first slice; the
priority is the disassembler so we can read what the compiler produced.

## Backend contract

A SPIR-V backend consuming RTIR:

1. Allocates SPIR-V ids by walking `type_constant_pool`, `global_variables`,
   `functions`, in order, remapping RTIR ids to SPIR-V ids.
2. Emits the SPIR-V header (magic, version, generator, id bound).
3. Emits `OpCapability` / `OpExtInstImport` for the `rt.std.450` set.
4. Emits `OpMemoryModel Logical GLSL450`.
5. Emits `OpEntryPoint` for each function in `entry_table`, listing the
   variables it references.
6. Emits `OpExecutionMode` (e.g. `OriginUpperLeft` for fragment).
7. Walks `decorations` and emits `OpDecorate` / `OpMemberDecorate`.
8. Emits the type/constant pool, then global variables, then functions.

Step (1) is the only nontrivial work. Steps (2)–(8) are mechanical.

## Status

The old IR (statement-string `BodyOp`) is being removed. The new IR is being
introduced incrementally:

1. New `IR/IR.h` data model (this commit).
2. New AST -> SSA lowering in `IR/IR.cpp`.
3. New `instruction_stream`, `global_variable_table`, `decoration_table`
   serialization in `Serialization/Artifact.cpp`.
4. New `Serialization/TextRTIR.cpp` SPIR-V-style disassembly.
5. New `Backend/SpirvBackend.cpp` emitting Vulkan SPIR-V.
6. `Backend/GlslBackend.cpp` is retired — Vulkan no longer goes through GLSL.

Until those land, only this design is current; the rest of the codebase still
reflects the old IR. Compute-stage support is intentionally out of scope for
the v0.1 surface.
