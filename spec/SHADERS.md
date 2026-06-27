# Rutile Shader Direction

Rutile's public shader language should be old-style GLSL: users write normal `in`, `out`,
`uniform` blocks, and sampler uniforms without Vulkan `set`/`binding` decorations. Backends
own the compile path. Internally, SPIR-V is still the best interchange format because Vulkan
can consume it directly, DX12 can transpile it to HLSL/DXIL, and Metal can transpile it to MSL.

## Proposed Pipeline

1. Rutile accepts GLSL source at `rtGraphicsProgramVertexShader` and `rtGraphicsProgramFragmentShader`.
2. Each backend normalizes layout-light GLSL into backend-usable GLSL:
   - normalize `#version` to the backend compiler target
   - assign vertex/input/output locations when missing
   - assign descriptor set/binding decorations when missing
   - translate compatibility tokens such as `attribute` and `varying`
3. Each backend compiles normalized GLSL to SPIR-V.
4. Vulkan consumes SPIR-V directly.
5. DX12 transpiles SPIR-V to HLSL and compiles that to the native shader bytecode.
6. Metal transpiles SPIR-V to MSL and builds the Metal shader library from that source.

The GLSL normalizer can be shared later, but the backend compiler boundary should stay backend-owned.
That lets each backend choose SPIRV-Cross, native compiler options, debug info settings, cache policy,
and platform-specific fixes without forcing those details into Rutile's public API.

## Current First Slice

- Vulkan stores GLSL source on the graphics program and compiles both graphics stages during link.
- Vertex/fragment varyings are linked by matching names before glslang sees either stage.
- Explicit modern layout declarations are still accepted as-is.
- Missing vertex input locations are assigned from `rt_vertex_layout` declaration order.
- Missing varying locations are assigned by the backend during `rtGraphicsProgramLink`.
- Missing uniform block and sampler bindings are assigned from stable uniform names so matching names
  across stages get the same binding.
- `attribute` becomes vertex `in`, and `varying` becomes vertex `out` or fragment `in`.
- Backend-chosen uniform bindings remain hidden behind `rt_uniform_location`; public shader code should
  stay name-based.

## Near-Term Milestones

1. Move the GLSL normalizer into a small shared module once DX12 starts using it.
2. Replace the DX12 hardcoded shader with a GLSL-to-SPIR-V-to-HLSL path.
3. Add a backend-neutral reflection query for linked uniforms, vertex inputs, and generated backend bindings.
4. Reflect SPIR-V into a backend-neutral resource layout.
5. Generate Vulkan descriptor set layouts and DX12 root signatures from the reflected layout.
6. Add a shader-cache key based on source bytes, normalized source version, backend target, and compile options.

## Open Questions

- Whether plain non-block uniforms should be rejected or packed into generated uniform blocks.
- Whether cross-stage varyings should eventually be matched by name instead of declaration order.
- Whether SPIRV-Cross is enough for every target or whether some backends need a second compiler path.
