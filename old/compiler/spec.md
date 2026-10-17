# Contents

- [Foreword](#foreword)
- [1 Scope](#intro.scope)
- [2 Normative references](#intro.refs)
- [3 Terms and definitions](#intro.defs)
  - [3.1 Consumable](#defns.consumable)
  - [3.2 Defined behavior](#defns.behavior.defined)
  - [3.3 Expression](#defns.expression)
  - [3.4 Identifier](#defns.identifier)
  - [3.5 Ill-formed program](#defns.ill.formed)
  - [3.6 Ill-formed, diagnostic required](#defns.ill.formed.diagnostic)
  - [3.7 Ill-formed, no diagnostic required](#defns.ill.formed.no.diagnostic)
  - [3.8 Implementation-defined behavior](#defns.behavior.implementation)
  - [3.9 Place](#defns.place)
  - [3.10 Undefined behavior](#defns.behavior.undefined)
  - [3.11 Unspecified behavior](#defns.behavior.unspecified)
  - [3.12 Well-formed program](#defns.well.formed)
- [4 General principles](#intro)
  - [4.1 Implementation compliance](#intro.compliance)
  - [4.2 Structure of this document](#intro.structure)
  - [4.3 Syntax notation](#syntax)
- [5 Lexical conventions](#lex)
  - [5.1 Keywords](#lex.key)
- [6 Basics](#basic)
  - [6.1 Type formation](#basic.types)
  - [6.2 Argument binding and consumption](#basic.arguments)
- [7 Expressions](#expr)
  - [7.1 Size queries](#expr.sizeof)
  - [7.2 Null pointer literals](#expr.nullptr)
  - [7.3 Allocation](#expr.new)
  - [7.4 Deallocation](#expr.delete)
  - [7.5 Discard](#expr.discard)
  - [7.6 Relocation](#expr.relocate)
  - [7.7 Explicit mutable access](#expr.access)
  - [7.8 Brace expressions](#expr.brace)
  - [7.9 Array expressions](#expr.array)
  - [7.10 Explicit casts](#expr.cast)
  - [7.11 No-op expressions](#expr.noop)
- [8 Statements](#stmt)
  - [8.1 Conditional statements](#stmt.if)
  - [8.2 While statements](#stmt.while)
  - [8.3 For statements](#stmt.for)
  - [8.4 Switch statements](#stmt.switch)
  - [8.5 Break statements](#stmt.break)
  - [8.6 Continue statements](#stmt.continue)
  - [8.7 Return statements](#stmt.return)
- [9 Declarations](#dcl)
  - [9.1 Variable declarations](#dcl.var)
  - [9.2 Function declarations](#dcl.fct)
  - [9.3 Const qualification](#dcl.const)
  - [9.4 Namespaces](#namespace)
  - [9.5 Using declarations](#dcl.using)
  - [9.6 Defaulted and deleted functions](#dcl.fct.def)
  - [9.7 Implicit conversions](#dcl.implicit)
- [10 User-defined types](#types.user)
  - [10.1 Structs and classes](#class)
  - [10.2 Unions](#class.union)
  - [10.3 Enumerations](#enum)
  - [10.4 Member access control](#class.access)
  - [10.5 Virtual functions](#class.virtual)
  - [10.6 Operator declarations](#over.oper)
  - [10.7 Move construction and move assignment](#class.move)
- [11 Templates](#temp)
  - [11.1 Template declarations](#temp.decl)
  - [11.2 Template parameters](#temp.param)

# Foreword

This document is a working specification. Numbered paragraphs state the rules established so
far. Examples illustrate those rules. Text labelled **To define** records unresolved design
questions and does not prescribe behavior. An unresolved rule is not an implicit choice of
undefined, unspecified, or implementation-defined behavior.

Grammar fragments specify established productions. Additional alternatives and operand
precedence remain incomplete where identified by an editorial **To define** entry.

<a id="intro.scope"></a>

# 1 Scope [intro.scope]

<sup>1</sup> This specification defines Rutile through rules governing program syntax, meaning,
and implementation behavior. An implementation conforms to Rutile by satisfying the applicable
requirements stated throughout this document, subject to the exceptions and allowances it
explicitly grants.

<sup>2</sup> Rutile is a general-purpose programming language inspired by C++, as described in
[N4986, *Working Draft, Programming Languages - C++*][cpp-draft]. Rutile aims to improve
upon C++ defaults, particularly in move semantics, and provides compile-time execution, type
arithmetic, and continuous compilation.

[cpp-draft]: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/n4986.pdf

<a id="intro.refs"></a>

# 2 Normative references [intro.refs]

**To define:** external documents whose requirements are incorporated into this specification,
the applicable editions, and the clauses that rely on them. The C++ draft cited in the scope
identifies a source of inspiration; that citation does not incorporate C++ rules into Rutile.

<a id="intro.defs"></a>

# 3 Terms and definitions [intro.defs]

<a id="defns.consumable"></a>

<div align="right"><a href="#defns.consumable">[defns.consumable]</a></div>

**3.1**<br>
**consumable**

An expression whose value can be consumed to initialize or replace an object.

<a id="defns.behavior.defined"></a>

<div align="right"><a href="#defns.behavior.defined">[defns.behavior.defined]</a></div>

**3.2**<br>
**defined behavior**

Behavior prescribed by this specification.

<a id="defns.expression"></a>

<div align="right"><a href="#defns.expression">[defns.expression]</a></div>

**3.3**<br>
**expression**

A language construct that computes a value, identifies storage, or performs an operation.

<a id="defns.identifier"></a>

<div align="right"><a href="#defns.identifier">[defns.identifier]</a></div>

**3.4**<br>
**identifier**

A token that names an entity, such as a function, variable, or class.

> *Note:* Name lookup determines which entity an identifier refers to in a given
> context.

<a id="defns.ill.formed"></a>

<div align="right"><a href="#defns.ill.formed">[defns.ill.formed]</a></div>

**3.5**<br>
**ill-formed program**

A program that violates a syntactic or semantic requirement of this specification.

<a id="defns.ill.formed.diagnostic"></a>

<div align="right"><a href="#defns.ill.formed.diagnostic">[defns.ill.formed.diagnostic]</a></div>

**3.6**<br>
**ill-formed, diagnostic required**

An ill-formed program for which the implementation must issue a diagnostic message.

> *Note:* A diagnostic requirement does not imply a requirement to reject the program.

<a id="defns.ill.formed.no.diagnostic"></a>

<div align="right">
<a href="#defns.ill.formed.no.diagnostic">[defns.ill.formed.no.diagnostic]</a>
</div>

**3.7**<br>
**ill-formed, no diagnostic required**

An ill-formed program for which the implementation is not required to detect or report the
violation.

<a id="defns.behavior.implementation"></a>

<div align="right">
<a href="#defns.behavior.implementation">[defns.behavior.implementation]</a>
</div>

**3.8**<br>
**implementation-defined behavior**

Behavior chosen by the implementation where this specification requires that choice to be
documented.

<a id="defns.place"></a>

<div align="right"><a href="#defns.place">[defns.place]</a></div>

**3.9**<br>
**place**

An expression that identifies an object's storage.

<a id="defns.behavior.undefined"></a>

<div align="right"><a href="#defns.behavior.undefined">[defns.behavior.undefined]</a></div>

**3.10**<br>
**undefined behavior**

Behavior for which this specification imposes no requirements.

<a id="defns.behavior.unspecified"></a>

<div align="right"><a href="#defns.behavior.unspecified">[defns.behavior.unspecified]</a></div>

**3.11**<br>
**unspecified behavior**

Behavior selected from alternatives permitted by this specification, without a requirement to
document which alternative is selected.

<a id="defns.well.formed"></a>

<div align="right"><a href="#defns.well.formed">[defns.well.formed]</a></div>

**3.12**<br>
**well-formed program**

A program that satisfies the syntactic and semantic requirements of this specification.

<a id="intro"></a>

# 4 General principles [intro]

<a id="intro.compliance"></a>

## 4.1 Implementation compliance [intro.compliance]

**To define:** which violations require diagnostics; whether a diagnostic requires rejection;
implementation limits; permitted extensions; and the obligations of a conforming implementation.
The behavior categories in [Terms and definitions](#intro.defs) do not assign individual language
rules to those categories. Such assignments must be stated explicitly.

<a id="intro.structure"></a>

## 4.2 Structure of this document [intro.structure]

<sup>1</sup> Clauses and subclauses in this document have stable names, shown in square brackets
beside their headings. These names identify sections independently of their numbering and remain
unchanged when sections are renumbered. References to sections use these stable names as link
targets.

<sup>2</sup> Language syntax is distinguished typographically from ordinary prose. Grammar terms
are written in italics, and literal keywords, operators, and other source text are written in
monospace. The notation is described in [4.3 Syntax notation](#syntax).

<a id="syntax"></a>

## 4.3 Syntax notation [syntax]

<sup>1</sup> Grammar productions use the following typographic conventions:

- A production begins with its name in italics, followed by a colon.
- Each alternative appears on a separate indented line.
- Grammar names are written in italics, such as <i>expression</i>.
- Literal source tokens are written in monospace, such as `if`.
- A subscript <sub>opt</sub> marks an optional element, such as
  <i>expression</i><sub>opt</sub>.

<sup>2</sup> A grammar name denotes a syntactic category. A literal denotes the tokens that
shall occur in the source. An alternative is formed by substituting a production for each
grammar name and retaining each literal token. Semantic constraints apply in addition to the
grammar.

<a id="lex"></a>

# 5 Lexical conventions [lex]

<a id="lex.key"></a>

## 5.1 Keywords [lex.key]

The following keyword list is the working inventory for this specification.

| Keyword | Feature section |
| --- | --- |
| `break` | [stmt.break](#stmt.break) |
| `case` | [stmt.switch](#stmt.switch) |
| `class` | [class](#class) |
| `const` | [dcl.const](#dcl.const) |
| `const_cast` | [expr.cast](#expr.cast) |
| `continue` | [stmt.continue](#stmt.continue) |
| `default` | [stmt.switch](#stmt.switch) |
| `delete` | [expr.delete](#expr.delete) |
| `discard` | [expr.discard](#expr.discard) |
| `dynamic_cast` | [expr.cast](#expr.cast) |
| `else` | [stmt.if](#stmt.if) |
| `enum` | [enum](#enum) |
| `final` | [class.virtual](#class.virtual) |
| `fn` | [dcl.fct](#dcl.fct) |
| `for` | [stmt.for](#stmt.for) |
| `if` | [stmt.if](#stmt.if) |
| `implicit` | [dcl.implicit](#dcl.implicit) |
| `namespace` | [namespace](#namespace) |
| `new` | [expr.new](#expr.new) |
| `nullptr` | [expr.nullptr](#expr.nullptr) |
| `operator` | [over.oper](#over.oper) |
| `override` | [class.virtual](#class.virtual) |
| `private` | [class.access](#class.access) |
| `protected` | [class.access](#class.access) |
| `public` | [class.access](#class.access) |
| `reinterpret_cast` | [expr.cast](#expr.cast) |
| `return` | [stmt.return](#stmt.return) |
| `sizeof` | [expr.sizeof](#expr.sizeof) |
| `static_cast` | [expr.cast](#expr.cast) |
| `struct` | [class](#class) |
| `switch` | [stmt.switch](#stmt.switch) |
| `template` | [temp.decl](#temp.decl) |
| `typename` | [temp.param](#temp.param) |
| `union` | [class.union](#class.union) |
| `using` | [dcl.using](#dcl.using) |
| `var` | [dcl.var](#dcl.var) |
| `virtual` | [class.virtual](#class.virtual) |
| `while` | [stmt.while](#stmt.while) |

`default` and `delete` also appear in
[defaulted and deleted functions](#dcl.fct.def).

**To define:** reservation rules and whether any words have context-specific status.

<a id="basic"></a>

# 6 Basics [basic]

<sup>1</sup> A declaration introduces an entity. An identifier names an entity; name lookup
determines the entity denoted by an occurrence of that identifier.

<sup>2</sup> An identifier expression that denotes an object variable is a
[place](#defns.place) identifying that object's storage. An identifier expression that denotes
a reference variable is a place identifying the storage of the object to which the reference
is bound.

**To define:** entity categories; object and storage definitions; reference binding; declaration
order and name lookup; initialization without an initializer; and object lifetime boundaries.

<a id="basic.types"></a>

## 6.1 Type formation [basic.types]

<p><i>type:</i><br>
&emsp;<i>type-name</i><br>
&emsp;<i>struct-specifier</i><br>
&emsp;<i>type</i> <code>*</code></p>

<sup>1</sup> A type-name denotes a type. A struct-specifier denotes the type declared by that
specifier ([10.1](#class)). For a type <i>T</i>, the type form <i>T</i>`*` denotes
pointer to <i>T</i>.

**To define:** remaining type productions; qualification; references; arrays; function types;
and restrictions on pointer formation. The productions above specify only the established
type forms.

<a id="basic.arguments"></a>

## 6.2 Argument binding and consumption [basic.arguments]

<sup>1</sup> A by-value parameter takes ownership of the object supplied by a consumable
argument. This transfer is called consumption. Cleanup responsibility transfers with ownership;
the argument result is not destroyed separately at the end of the calling statement.

<sup>2</sup> Binding an argument to a source parameter of type `const T&`
preserves the source object's lifetime. Mutable reference binding requires explicit mutable
access ([7.7](#expr.access)).

**To define:** parameter initialization; conversions before binding; parameter lifetime;
return-value transfer; and argument evaluation order.

<a id="expr"></a>

# 7 Expressions [expr]

<p><i>expression:</i><br>
&emsp;<i>identifier-expression</i><br>
&emsp;<i>null-pointer-literal</i><br>
&emsp;<i>discard-expression</i><br>
&emsp;<i>relocation-expression</i><br>
&emsp;<i>access-expression</i><br>
&emsp;<i>brace-expression</i><br>
&emsp;<i>array-expression</i><br>
&emsp;<i>cast-expression</i><br>
&emsp;<i>variable-declaration-expression</i><br>
&emsp;<i>no-op-expression</i></p>

<sup>1</sup> Expressions are subject to the type, value-category, and evaluation constraints
of the context in which they occur.

<sup>2</sup> An expression used as the body of a construct is a body expression. The enclosing
construct establishes its interpretation context, including the declarations in scope and the
role of declarations introduced within the body.

**To define:** complete expression grammar; types and value categories; operand requirements;
precedence and associativity; evaluation order; conversions; result lifetimes; and constant
evaluation. In particular, whether places and consumables exhaust the value categories and how
function names are classified remain undecided. Individual expression forms and their rules are
specified in the following subclauses as they are defined.

<a id="expr.sizeof"></a>

## 7.1 Size queries [expr.sizeof]

**To define:** operand forms; whether operands are evaluated; result type; and size and alignment
rules.

<a id="expr.nullptr"></a>

## 7.2 Null pointer literals [expr.nullptr]

<p><i>null-pointer-literal:</i><br>
&emsp;<code>nullptr</code></p>

<sup>1</sup> A null-pointer-literal denotes the null pointer value of type `null*`.

<sup>2</sup> No object shall have type `null`. Forming the pointer type
`null*` is permitted.

<sup>3</sup> An implicit conversion exists from `nullptr` to every other pointer
type. No implicit conversion exists from `void*` to another pointer type; such a
conversion requires an explicit cast.

**To define:** the value category of `nullptr`; conversion rules for other values of
type `null*`; conversions from other pointer types to `null*`;
comparisons; dereference rules; explicit pointer-cast syntax; and the permitted uses of
`null` elsewhere in the type system.

<a id="expr.new"></a>

## 7.3 Allocation [expr.new]

<sup>1</sup> An allocation expression obtains storage through `operator new` or
`operator new[]`. A custom allocation operator shall be a member of an allocator type.
A global allocation operator shall not be overloaded.

<sup>2</sup> Allocation precedes construction. The allocation expression constructs the
allocated objects in the storage supplied by the allocation operator.

**To define:** allocated types; initialization; allocation failure; array sizes; and ownership of
the result.

**To define:** allocator selection syntax; member operator signatures; size and alignment
arguments; default allocation; allocator lifetime; and how the matching allocator is identified
when storage is released.

<a id="expr.delete"></a>

## 7.4 Deallocation [expr.delete]

<sup>1</sup> A deallocation expression releases storage through `operator delete` or
`operator delete[]`. A custom deallocation operator shall be a member of an allocator type.
A global deallocation operator shall not be overloaded.

<sup>2</sup> The language deallocation operation destroys the objects before invoking the
allocator's operation to release their storage.

**To define:** accepted pointer types; null operands; destructor calls; array deallocation; and
invalid pointers.

**To define:** member operator signatures; matching allocation and deallocation operations;
allocator identity; and storage release if construction or destruction fails.

<a id="expr.discard"></a>

## 7.5 Discard [expr.discard]

<p><i>discard-expression:</i><br>
&emsp;<code>discard</code> <i>expression</i></p>

<sup>1</sup> Evaluation of a discard-expression ends the lifetime of the operand's object
without invoking its destructor.

<sup>2</sup> The operand shall be a consumable. A discard-expression whose operand is a
[place](#defns.place) is ill-formed.

```rutile
Foo a;
discard a; // Ill-formed: a is a place.
```

```rutile
Foo a;
discard $a; // Relocate a, then discard the consumable without destruction.
```

> *Note:* If `a` is declared as `Foo&`,
> `discard a;` is also ill-formed because `a` is a place.

**To define:** result type and category of the discard expression, precedence, and operand
evaluation rules; and operand grouping in the grammar production.

<a id="expr.relocate"></a>

## 7.6 Relocation [expr.relocate]

<p><i>relocation-expression:</i><br>
&emsp;<code>$</code> <i>expression</i></p>

<sup>1</sup> The `$` operator relocates the source object into the result of the
expression. The result is a [consumable](#defns.consumable). The source object's lifetime ends
when relocation occurs.

<sup>2</sup> Destruction of the result is scheduled at the end of the containing statement.
Consumption by a by-value parameter transfers cleanup responsibility as specified in
[6.2](#basic.arguments). Discard ends the result's lifetime as specified in paragraph 3.

<sup>3</sup> If the result is consumed by [discard](#expr.discard), its lifetime ends through
that operation without a destructor call. It is not subsequently destroyed at statement end.

<sup>4</sup> Indirection does not prevent relocation. A relocation operand may designate an
object through dereference of a pointer to that object's type. The lifetime that ends is the
lifetime of the designated object; the pointer object's lifetime is unaffected. The operand is
evaluated once. A const-qualified object shall not be relocated.

```rutile
Foo foo;
$foo;
```

> *Example:* In `$foo;`, relocation ends the source object's lifetime.
> The result is not passed to another operation and is destroyed at the semicolon.

**To define:** other eligible source objects; relocation through references or subobjects;
precedence; source access after relocation; and source storage reuse. Return-value transfer and
direct transfer into destination storage; and constructor invocation during relocation.

<a id="expr.access"></a>

## 7.7 Explicit mutable access [expr.access]

<p><i>access-expression:</i><br>
&emsp;<code>@</code> <i>expression</i></p>

<sup>1</sup> A place expression naming an object of type <i>T</i>, without explicit mutable
access, binds to `const T&`. It shall not bind to `T&`.

<sup>2</sup> For a place <i>e</i> naming a mutable object of type <i>T</i>,
`@`<i>e</i> provides explicit mutable access and binds to `T&`.
For a place naming a const object, the result retains the object's const qualification.
The `@` operator shall not remove const qualification.

```rutile
Foo foo;
const Foo& read = foo;
Foo& write = @foo;
```

**To define:** operand restrictions, precedence, and interaction with member access and calls.

<a id="expr.brace"></a>

## 7.8 Brace expressions [expr.brace]

<p><i>brace-expression:</i><br>
&emsp;<code>{</code> <i>brace-content</i><sub>opt</sub> <code>}</code></p>

<sup>1</sup> The brace-content is evaluated as part of the enclosing brace-expression.

<sup>2</sup> The brace-content has the interpretation context of the brace-expression
([7](#expr)).

<sup>3</sup> A standalone brace expression provides a scope for its enclosed contents.

**To define:** brace-content grammar; result type and value category; scope and lifetime
boundaries; and statement termination, including when a trailing semicolon is required.

<a id="expr.array"></a>

## 7.9 Array expressions [expr.array]

<sup>1</sup> An array expression is a built-in expression form containing a comma-separated
sequence of elements enclosed by `[` and `]`. Its type is inferred without
requiring a destination type. The number of elements determines its array length.

```rutile
char[4] foo = ['a', 'b', 'c', 'd'];
```

> *Example:* The array expression above has type `char[4]`, independently
> of the declared type of `foo`.

**To define:** element-type inference for mixed types; empty arrays; trailing commas; evaluation
order; element consumption; result value category; and array returns. Conversion to a
pointer-and-count view and whether that view is built-in or supplied by the standard library
remain undecided.

<a id="expr.cast"></a>

## 7.10 Explicit casts [expr.cast]

<sup>1</sup> Explicit casts use the named forms `static_cast`,
`dynamic_cast`, `reinterpret_cast`, and `const_cast`.
Each form specifies a destination type in angle brackets and an operand in parentheses.

<p><i>cast-expression:</i><br>
&emsp;<code>static_cast&lt;</code> <i>type</i> <code>&gt;(</code>
<i>expression</i> <code>)</code><br>
&emsp;<code>dynamic_cast&lt;</code> <i>type</i> <code>&gt;(</code>
<i>expression</i> <code>)</code><br>
&emsp;<code>reinterpret_cast&lt;</code> <i>type</i> <code>&gt;(</code>
<i>expression</i> <code>)</code><br>
&emsp;<code>const_cast&lt;</code> <i>type</i> <code>&gt;(</code>
<i>expression</i> <code>)</code></p>

<sup>2</sup> A parenthesized type followed by an expression does not form a cast.
Explicit casts shall use a named cast form. Parentheses enclosing an expression group that
expression without performing a conversion.

**To define:** the conversions permitted by each named cast; runtime checks and failure
behavior; const qualification; result categories and lifetimes; and interaction with source
conversion operators. Adopting these spellings does not incorporate C++ cast semantics.

<a id="expr.noop"></a>

## 7.11 No-op expressions [expr.noop]

<p><i>no-op-expression:</i><br>
&emsp;<code>;</code></p>

<sup>1</sup> Evaluation of a no-op-expression performs no operation.

<sup>2</sup> A function or type body consisting of a no-op-expression does not supply a
definition. The enclosing declaration declares the entity without defining it.

<sup>3</sup> A brace-expression is not a no-op-expression, including when its brace-content
is absent.

**To define:** result type and category; and how the semicolon of a no-op-expression interacts
with declaration and statement termination.

<a id="stmt"></a>

# 8 Statements [stmt]

**To define:** statement grammar and its relationship to expressions and declarations; expression
statement termination; and statement-end cleanup.

<a id="stmt.if"></a>

## 8.1 Conditional statements [stmt.if]

<p><i>if-statement:</i><br>
&emsp;<code>if (</code> <i>condition</i> <code>)</code> <i>expression</i><br>
&emsp;<code>if (</code> <i>condition</i> <code>)</code> <i>expression</i>
<code>else</code> <i>expression</i></p>

<sup>1</sup> The condition is evaluated before either body expression. If the condition is
true, the first expression is evaluated. Otherwise, the expression following `else`
is evaluated if present. Only the selected expression is evaluated.

<sup>2</sup> The if-statement introduces no names into either body expression.

**To define:** condition grammar and conversions; nested-else association; statement terminators;
and whether initializer or compile-time conditional forms are provided.

<a id="stmt.while"></a>

## 8.2 While statements [stmt.while]

**To define:** condition syntax and conversions; body expressions; repetition; and break and
continue behavior.

<a id="stmt.for"></a>

## 8.3 For statements [stmt.for]

<sup>1</sup> The body of a `for` is an expression. Identifiers declared by the
initializer are available in the body expression.

**To define:** complete syntax, execution order, and the scope and lifetime of initializer
declarations in the condition, iteration expression, and body.

<a id="stmt.switch"></a>

## 8.4 Switch statements [stmt.switch]

**To define:** selector types; case matching; default selection; fallthrough; and case scope.

<a id="stmt.break"></a>

## 8.5 Break statements [stmt.break]

**To define:** eligible enclosing constructs; target selection; and cleanup on exit.

<a id="stmt.continue"></a>

## 8.6 Continue statements [stmt.continue]

**To define:** eligible loops; target selection; iteration-expression execution; and cleanup.

<a id="stmt.return"></a>

## 8.7 Return statements [stmt.return]

**To define:** return operands; reference and value returns; consumption; and cleanup before
caller resumption.

<a id="dcl"></a>

# 9 Declarations [dcl]

<a id="dcl.var"></a>

## 9.1 Variable declarations [dcl.var]

<p><i>variable-declaration-expression:</i><br>
&emsp;<code>var</code> <i>identifier</i> <code>:</code> <i>type</i><br>
&emsp;<code>var</code> <i>identifier</i> <code>=</code> <i>expression</i><br>
&emsp;<i>type</i> <i>identifier</i></p>

<p><i>variable-declaration:</i><br>
&emsp;<i>variable-declaration-expression</i> <code>;</code></p>

<sup>1</sup> A variable-declaration-expression introduces a variable named by its identifier.
In the annotated form, the type following <code>:</code> specifies the variable's type. In the
initializer form, the variable's type is deduced from the initializer expression.

<sup>2</sup> The expression <i>T name</i> is syntactic sugar for
<code>var</code> <i>name</i> <code>:</code> <i>T</i>. Both forms declare the same
variable with the same type and initialization semantics.

<sup>3</sup> Constructor-based initialization of an object of type <i>D</i> selects a constructor
of <i>D</i> whose source parameter accepts the initializer. Argument binding is specified in
[6.2](#basic.arguments).

> *Example:*

```rutile
struct {}* a;
```

> The struct-specifier denotes a struct type; pointer formation yields the declared
> type of `a`.

**To define:** declaration grammar; multiple declarations sharing a type; initialization forms;
type deduction details; annotated declarations with initializers; corresponding type-first
forms; initialization of built-in types; and restrictions on anonymous types.

<a id="dcl.fct"></a>

## 9.2 Function declarations [dcl.fct]

<p><i>function-declaration:</i><br>
&emsp;<code>fn</code> <i>function-name</i> <i>function-signature</i>
<i>function-modifiers</i><sub>opt</sub> <i>expression</i></p>

<p><i>function-signature:</i><br>
&emsp;<code>(</code> <i>parameter-list</i><sub>opt</sub> <code>)</code>
<i>return-clause</i><sub>opt</sub></p>

<p><i>return-clause:</i><br>
&emsp;<code>-&gt;</code> <i>type</i></p>

<sup>1</sup> Function parameters are available by name within the function's body expression.

<sup>2</sup> A function declaration has a body expression. A no-op-expression body declares
the function without defining it ([7.11](#expr.noop)); any other body supplies its definition.

**To define:** declaration and definition grammar; parameters and receivers; body requirements;
return types; argument passing; linkage; and overload resolution.

<a id="dcl.const"></a>

## 9.3 Const qualification [dcl.const]

**To define:** qualification syntax; qualification of compound types; deduction; and permitted
conversions.

<a id="namespace"></a>

## 9.4 Namespaces [namespace]

**To define:** declarations; reopening; qualified lookup; nesting; and interaction with templates.

<a id="dcl.using"></a>

## 9.5 Using declarations [dcl.using]

**To define:** accepted entities; aliases versus imported names; lookup; duplicates; and cycles.

<a id="dcl.fct.def"></a>

## 9.6 Defaulted and deleted functions [dcl.fct.def]

**To define:** eligible functions; declaration syntax; generation conditions; and use of deleted
operations.

<a id="dcl.implicit"></a>

## 9.7 Implicit conversions [dcl.implicit]

<sup>1</sup> A constructor marked `implicit` is eligible for implicit conversion to its
destination type. Eligibility shall not require the source and destination types to differ.

<sup>2</sup> Conversion through an implicit constructor binds the source to its parameter
according to [6.2](#basic.arguments) and produces a consumable of the destination type.

> *Example:* An implicit constructor of `rt::string_view` with a source parameter of type
> `const rt::string&` supplies conversion from a string place to a string-view consumable.

<sup>3</sup> For an implicit conversion from a source type to a destination type, construction
through the destination type's applicable implicit constructors is considered first. A source
conversion operator is considered only when no applicable destination constructor is available.

<sup>4</sup> An explicit user-defined conversion selects a conversion operator of the source
type rather than using destination construction. Constructor invocation and explicit casting are
distinct operations even when both produce a value of the same destination type.

**To define:** complete declaration syntax, including constructor return spelling; eligible
conversion contexts; candidate selection and ranking; permitted conversion chains; and the
lifetime requirements of views produced by conversion; and types with implicit copying enabled
by default.

**To define:** constructor ambiguity and deleted or inaccessible candidates; eligibility of
implicit conversion operators; conversions without user-defined functions;
and the result when an explicit cast has no applicable source conversion operator.

<a id="types.user"></a>

# 10 User-defined types [types.user]

<a id="class"></a>

## 10.1 Structs and classes [class]

<p><i>struct-specifier:</i><br>
&emsp;<code>struct</code> <i>identifier</i><sub>opt</sub> <i>expression</i></p>

<sup>1</sup> A struct-specifier declares and denotes a struct type. Its identifier, if present,
names that type. Its expression is the body of the type.

<sup>2</sup> A no-op-expression body does not define the type ([7.11](#expr.noop)). Any other
body defines the type. Variable declarations in the body's type context declare data members.

> *Example:*

```rutile
struct Foo {
    i32 value;
} foo;
```

> The declaration of `foo` follows from the variable-declaration production
> with the struct-specifier as its type.

**To define:** declaration and redeclaration lookup; completeness requirements; complete grammar;
type-declaration termination; body-expression boundaries; members; struct and class differences;
layout; and inheritance.

<a id="class.union"></a>

## 10.2 Unions [class.union]

**To define:** member storage; active-member tracking; initialization; member access; and
destruction.

<a id="enum"></a>

## 10.3 Enumerations [enum]

**To define:** underlying types; enumerator values; scope; conversions; and valid operations.

<a id="class.access"></a>

## 10.4 Member access control [class.access]

**To define:** public, private, and protected access; defaults; inheritance; and enforcement
contexts.

<a id="class.virtual"></a>

## 10.5 Virtual functions [class.virtual]

**To define:** virtual declaration syntax; override matching; final restrictions; dispatch; and
destruction.

<a id="over.oper"></a>

## 10.6 Operator declarations [over.oper]

<sup>1</sup> An operator name consists of the keyword `operator` followed by the
operator spelling. Whitespace between them is optional unless needed to separate lexical
tokens. In particular, word spellings such as `new` and `delete` require
separation from the keyword.

```rutile
operator+       // Same name as operator +
operator[]      // Same name as operator []
operator()      // Same name as operator ()
operator new
operator delete
operator new[]
operator delete[]
```

<sup>2</sup> A conversion operator name consists of `operator` followed by its
destination type, as in `operator Foo` or
`operator rt::string_view`.

> *Note:* A naming convention does not by itself make every possible operator
> overloadable. The set of permitted operator spellings is specified separately.

**To define:** overloadable operators; declaration syntax; operands; candidate discovery; and
ranking.

<a id="class.move"></a>

## 10.7 Move construction and move assignment [class.move]

<sup>1</sup> A constructor or assignment function with a by-value source parameter is a
move constructor or move assignment function, respectively. A constructor or assignment
function with a source parameter of type `const T&` is a copy constructor or
copy assignment function, respectively. In either case, <i>T</i> is the source type and need
not be the destination type.

<sup>2</sup> Source argument binding is specified in [6.2](#basic.arguments).

<sup>3</sup> A compiler-generated move constructor copies each member of its source parameter
into the corresponding member of the destination object. Members are copied, not moved.
After the copies, the source parameter is discarded without invoking its destructor.

<sup>4</sup> Compiler-generated move assignment first invokes the destination object's
destructor, ending that object's lifetime, and then invokes the move constructor to construct
its replacement in the same storage. Destruction precedes reconstruction. The generated body
follows this sequence:

```rutile
this.~Foo();
this.Foo($other);
```

> *Note:* The sequence illustrates the operation order; constructor return syntax
> and explicit receiver-access spelling remain undecided.

<sup>5</sup> Paragraphs 3 and 4 specify compiler-generated definitions. A user-provided
definition executes its declared body according to the function execution rules.

> *Note:* The source members need not be cleared merely to prevent the discarded
> source's destructor from releasing the transferred resources.

**To define:** declaration syntax, conditions for compiler-generated operations, member-copy
semantics and order; base subobjects; self-assignment; and failure during destruction, member
copying, or reconstruction. In particular, whether member copying invokes copy constructors or
copies representations has not been decided. Parameter lifetime and destruction when a
user-written function does not discard its source also require general function rules.

<a id="temp"></a>

# 11 Templates [temp]

<a id="temp.decl"></a>

## 11.1 Template declarations [temp.decl]

**To define:** eligible declarations; definition checking; instantiation; specialization; and
identity.

<a id="temp.param"></a>

## 11.2 Template parameters [temp.param]

**To define:** parameter kinds; typename and class roles; defaults; deduction; and argument
validity.

