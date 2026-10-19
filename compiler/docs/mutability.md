# Mutability

A value type is mutable by default. The `const` qualifier specifies an immutable type.
The target of a pointer or reference is const by default; `var` permits a mutable target.
Qualification of a pointer object is independent of qualification of its target.
Target qualification governs access through that pointer or reference; the original object's
qualification is unchanged.

An explicit `var` qualifier suppresses default readonly qualification at its written position.
An existing explicit const qualification remains in effect.

| Type | Object | Pointed-to object |
| --- | --- | --- |
| `Foo*` | mutable pointer | const `Foo` |
| `var Foo*` | mutable pointer | mutable `Foo` |
| `const (Foo*)` | const pointer | const `Foo` |
| `const (var Foo*)` | const pointer | mutable `Foo` |

`Foo&` refers to a const `Foo`; `var Foo&` refers to a mutable `Foo`.

A declaration using an unqualified value type declares a mutable object:

```rutile
Foo foo;
var inferred = Foo();
```

Template arguments follow these same rules. `buffer<i32>` contains mutable integers;
`buffer<i32*>` contains mutable pointers whose integer targets are const by default.
Neither template substitution nor container storage introduces a separate qualification default.

`auto` retains its existing qualification deduction rules. Conditional qualification follows the
named parameter's qualification, using `const[name]` or `var[name]`.
