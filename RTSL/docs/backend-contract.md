# RTSL Backend Contract

Rutile backends consume linked `rtslp` artifacts. Backends do not parse RTSL
source and do not consume textual RTIR in production paths.

## Backend Inputs

A backend receives a binary `rtslp` containing:

- final RTIR instruction stream
- resolved function table
- entry point metadata
- resource metadata
- stage-interface metadata
- type and constant tables
- symbol names and debug metadata
- primitive call references

The backend may reject an `rtslp` if it uses unsupported stage families,
resource kinds, primitive operations, type layouts, or target capabilities.

## Entry Metadata

Each entry point record includes:

- preserved authored name
- canonical symbol identity
- function id
- stage family
- parameter and return payload types
- source/debug origin
- required resource and stage-interface references

Backends use this metadata to select the appropriate target shader stage and
entry name.

## Resource Metadata

Resource metadata includes:

- uniform scope names
- resource names
- resource types
- access qualifiers such as `readonly` and `writeonly`
- compiler-assigned binding metadata
- source-facing reflection names

Backends may map this metadata to descriptor sets, register spaces, argument
buffers, or other target-specific binding systems.

## Stage Interface Metadata

Stage metadata includes:

- input and output payload types
- `varying` declarations
- interpolation qualifiers such as `clip`, `smooth`, and `flat`
- stage-family-specific payload data
- compatibility information between connected stages

Backends use this metadata to lower stage inputs and outputs to the target
language or binary format.

## Primitive Lowering

Primitive calls are identified by canonical symbols under `rt::__primitive`.
Backends lower primitives to target operations.

Examples include:

- texture sampling
- buffer and image loads/stores when not expressible as ordinary memory ops
- barriers
- derivatives
- discard
- subgroup operations
- ray tracing operations

Ordinary standard library functions are not backend intrinsics unless they call
reserved primitives.

## Debug Metadata

Backends should preserve debug names and source mappings where the target format
supports them. At minimum, the backend must not require names to be stripped for
correctness.

Debug metadata may include source files, line/column spans, symbol origins,
instruction origins, preserved display names, and generated temporary names.

## Backend Outputs

A backend may lower `rtslp` to SPIR-V, HLSL, MSL, WGSL, or another
backend-specific representation. Backend output is outside the RTSL artifact
family and may have target-specific reflection or packaging.

The backend contract is intentionally based on RTIR, metadata tables, and
primitive identities so new target formats can be added without changing the
source language.
