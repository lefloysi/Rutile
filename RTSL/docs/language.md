# RTSL Language Semantics

This document defines the source language accepted by the RTSL compiler. RTSL is
a graphics-first shader language for Rutile. For v0.1, source files compile to
`rtslo` objects and optional `rtslm` interfaces, and linked programs target the
vertex and fragment graphics stages.

## Lexical Model

RTSL source is UTF-8 text. The initial compiler implementation may restrict
identifiers to ASCII, but the format must preserve original source spelling for
diagnostics and debug output.

Whitespace separates tokens and is otherwise insignificant outside literals.
Line comments start with `//` and end at the next line break. Block comments
start with `/*` and end with `*/`.

Identifiers start with a letter or `_` and continue with letters, digits, or
`_`. Identifiers are case-sensitive. Keywords cannot be used as ordinary
identifiers.

Numeric literals include integer and floating-point forms. String literals use
double quotes. Character literals may be added later if needed by the standard
library or compile-time evaluation.

The core punctuators and operators include:

- grouping: `(`, `)`, `{`, `}`, `[`, `]`
- separators: `,`, `;`, `.`, `::`
- arithmetic: `+`, `-`, `*`, `/`, `%`
- assignment and comparison: `=`, `==`, `!=`, `<`, `<=`, `>`, `>=`
- logical and bitwise: `!`, `&&`, `||`, `&`, `|`, `^`, `~`
- type and function syntax: `->`, `<`, `>`

## Keywords

Modules and visibility: `import`, `export`

Declarations and scopes: `namespace`, `struct`, `using`, `uniform`, `varying`,
`input`, `output`

Functions and types: `fn`, `const`, `auto`, `void`

Control flow: `if`, `else`, `while`, `do`, `for`, `return`

Stage interface qualifiers: `clip`, `smooth`, `flat`, `location`, `builtin`

Resource access qualifiers: `readonly`, `writeonly`

Boolean literals: `true`, `false`

Parameter and payload qualifiers may include `inout` for stages or APIs that
require writable payload parameters.

## v0.1 Scope

The first release is intentionally narrow. Supported v0.1 features are:

- source compilation to `rtslo`
- optional `rtslm` interface emission
- linked `rtsll` libraries and `rtslp` programs
- uniform reflection
- stage-interface reflection
- vertex and fragment entry points
- textual RTIR disassembly and assembly for inspection

The following are documented goals, but not release guarantees for v0.1:

- import search paths and full multi-file dependency resolution
- backend-agnostic primitive expansion beyond the current graphics path
- broader stage families beyond vertex and fragment
- a stable long-term artifact ABI

## Translation Units

Every `.rtsl` file is a translation unit and may define private symbols,
exported symbols, entry points, resource scopes, and stage payload contracts.
There are no user-authored header files.

Forward declarations are allowed for local ordering convenience. They do not
form an import interface. Import interfaces come from `rtslm` files.

## Imports And Exports

Imports are file-oriented:

```rtsl
import <math.rtsl>;
import <lighting/brdf.rtsl>;
```

The source-like import path resolves to a compiled module interface:

```text
math.rtsl -> math.rtslm
lighting/brdf.rtsl -> lighting/brdf.rtslm
```

`export` marks a symbol as visible through the emitted `rtslm` interface. A
source file with no exports still emits `rtslo`, but does not need to emit
`rtslm`.

## Namespaces And Name Lookup

`namespace` introduces a qualified scope. `::` selects a member of a namespace,
uniform scope, type scope, or other qualified symbol container.

Name lookup first searches the innermost lexical scope, then outer lexical
scopes, then imported/exported scopes made visible by `using`.

Function identity is not the short name alone. The canonical identity is:

```text
qualified_name + parameter_types + return_type
```

The original display name is preserved for diagnostics, disassembly, and debug
symbols.

The namespace `rt::__primitive` is reserved. User code cannot define, shadow,
alias, import over, or export symbols in that namespace.

## Type Declarations

`struct` defines aggregate data and may declare constructors or methods:

```rtsl
struct Vertex {
    Vertex(Point p);
    vec4 position;
    vec2 uv;
};
```

Methods are defined with qualified names:

```rtsl
fn Vertex::Vertex(Point p) {
    position = vec4(p.position, 1.0);
}
```

Builtin scalar, vector, matrix, resource, and stage helper types are provided by
the compiler and standard library. For v0.1, the supported surface is limited to
plain scalar/vector/matrix types, `Sampler2D`, and the built-in graphics stage
carriers.

## Functions And Statements

Functions use `fn`:

```rtsl
fn saturate(f32 x) -> f32 {
    return clamp(x, 0.0, 1.0);
}
```

If no return type is written, the function returns `void`.

Statements include declarations, expression statements, blocks, `if`, `else`,
`while`, `do`, `for`, and `return`.

Expressions include literals, names, qualified names, calls, constructors, field
access, indexing, unary operators, binary operators, assignment, and compound
assignment.

## Uniforms And Resources

`uniform` declares a resource binding scope:

```rtsl
uniform albedo {
    Sampler2D texture;
    vec4 tint;
}
```

Named uniform members are accessed through the uniform scope:

```rtsl
sample(albedo::texture, uv) * albedo::tint
```

Anonymous uniform scopes expose their members directly in the surrounding
namespace. Reopened named uniform scopes are merged. Nested `uniform` scopes are
invalid.

Resource declarations may use access qualifiers:

```rtsl
uniform particles {
    readonly Sampler2D texture;
    vec4 tint;
}
```

The compiler assigns backend binding numbers and records the reflected resource
layout in artifacts.

## Stage Interfaces

Three declarations describe how a payload type crosses a stage boundary:

- `input` — host-supplied stage inputs (e.g. vertex attributes)
- `varying` — values interpolated from one stage to the next
- `output` — final stage outputs (e.g. framebuffer attachments)

```rtsl
input Point {
    location(0) position;
    location(1) uv;
}
varying Vertex {
    clip position;
    smooth uv;
    flat material;
}
output Fragment {
    location(0) color;
}
```

Field qualifiers may appear in any order before the field name:

- `smooth` / `flat` control interpolation.
- `clip` marks the clip-space position; it is delivered through the rasterizer's
  built-in position slot and consumes no user location. It is a vertex output
  only and is not readable as an input on later stages.
- `location(N)` assigns an explicit binding location. Fields without an explicit
  location are numbered sequentially; built-in fields consume no location.
- `builtin(name)` routes a field to a named built-in slot.

Assigned locations are recorded in every artifact and are queryable through the
reflection ABI so hosts can bind inputs and read outputs.

## Entry Points

`fn` declarations whose names begin with the stage tag mark executable shader
entry points. The stage is derived from the function name's leading 4-letter
tag:

| Tag    | Stage    |
|--------|----------|
| `vert` | vertex   |
| `frag` | fragment |

```rtsl
fn vert_main(Point p) -> Vertex {
    return Vertex(p);
}
```

The compiler keeps the authored function as an ordinary function and generates a
backend entry wrapper named for the stage (`vert`, `frag`). This generated stage
runtime reads the stage inputs into the source-level input payload, calls the
authored function, and writes the result across the stage boundary using the
resolved interface metadata. The authored name is preserved for reflection and
debugging.

## Stage Builtins

Stage built-ins (`gl_Position`, `gl_PointSize`, `gl_VertexIndex`, `gl_FragCoord`,
`gl_FragDepth`, …) are delivered through a
**builtin carrier struct passed by reference** as the entry's first parameter:

```rtsl
fn vert_main(RtVertex& b, Point p) -> Vertex {
    b.position = mvp * vec4(p.position, 1.0);  // routes to gl_Position
    return Vertex(p);
}
```

Each stage has a carrier type — `RtVertex`, `RtFragment` — whose members map to
the stage's built-ins. Output members (e.g. `position` → `gl_Position`) are
written by the shader; input members (e.g. `vertex_index` → `gl_VertexIndex`)
are read-only. The carrier is passed by reference (`&`), so the generated stage
runtime copies the used inputs in before the call and the used outputs back to
the real `gl_*` globals after it. Built-ins the entry never touches are not
copied.

A linked `rtslp` must contain the entry points required by its program family:

- a **graphics** program must contain `vert` and `frag`.

The linker reports a diagnostic when a graphics program is missing a required
stage. Compute and advanced stage families are out of scope for v0.1 and are
rejected with a clear diagnostic if encountered.

## Standard Library And Primitives

The standard library should be written in RTSL where possible. Functions such as
`sample`, `normalize`, `saturate`, and `mix` are ordinary callable functions
unless they need primitive backend behavior.

Only operations that cannot be expressed in RTSL should use reserved primitives:
texture sampling, barriers, derivatives, discard, and other backend-specific
operations.
