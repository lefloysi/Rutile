# Inferred function facts and reference analysis

The compiler infers these facts from resolved function bodies. There is no public proof syntax.
The implementation is staged; this document distinguishes the working foundation from remaining
analysis work.

## Analysis pipeline

`InferredFacts` runs after semantic construction. `LifetimeAnalyzer` and `ReferenceLoanAnalyzer`
then check object lifetimes and tracked reference accesses. All three use `FactGraph` with
separate states. The frontend variant analysis remains in place.

The graph represents branches, short circuiting, loops, switches, returns, throws, and handlers.
Exception dispatch deliberately includes every potentially matching handler. Unsupported effects
lose information instead of establishing assumptions.

Owning locals and value parameters have explicit lifetime-begin and lifetime-end nodes. Reference
parameters do not end their external referents. Initialization, relocation, reconstruction,
destruction, and exceptional cleanup follow the backend's evaluation order. Numeric lifetime
invalidation removes storage facts while preserving values already copied or relocated elsewhere.

Places identify object roots and field projections. Numeric state versions distinguish values
before and after writes. The abstract domain includes integer intervals, exact negative values,
equalities and comparisons, overflow-checked affine offsets, and active variant alternatives.
Queries distinguish proven, disproven, and unknown conditions.

Function summaries contain symbolic entry-state expressions, path conditions, returned values,
and writes. Boolean and union outcomes retain their correlations with resulting state. Only facts
common to every corresponding return path are exported. Read-only wrappers compose summaries to
a fixed point; unresolved recursion and dynamically dispatched calls remain conservative.

Direct mutating calls also compose their normal outcomes through wrapper statements, returns,
local initializers, assignments, and conditions. Arguments are evaluated once; imported guards,
returned values, and write values use the frozen call-entry state before writes are installed.
Composition retains at most 64 paths. Exceeding the bound makes the summary opaque.

A direct throw ends its normal path. A scalar memory read can return an unknown value while
preserving the guards on that path. A read-only wrapper imports its callee's single normal
outcome, including those guards. This does not classify a throw as a compile-time violation.

Opaque calls, pointer writes, user special members without usable summaries, and uncertain alias
effects invalidate potentially affected facts. No method names identify getters or mutators.

## Observable behavior

- Provably invalid fixed-array accesses produce a compile-time diagnostic, including negative
  indices. A note reports the inferred index range and array extent. Uncertain accesses retain
  their existing runtime behavior.
- Proven scalar expressions become constants in native generation when evaluation has no
  observable effects. Checked variant extraction also consumes proven alternatives.
- A proven effectful Boolean condition still evaluates its calls and operands. Integer comparison
  and negation lowering then substitute the known result. Short-circuit expressions retain their
  control flow and cleanup boundaries.
- Repeated guards and relations learned across calls can disappear at optimization level zero.
- Guarded growth can establish that a new value exceeds its previous value. Unguarded wrapping
  arithmetic does not establish that relationship.
- Effect-free compiler-defaulted local record initialization establishes integer field values,
  including zero initialization, constant field initializers, and nested records. Custom
  constructors, reference-bearing storage, and overlapping layouts remain conservative.
- Local reference loans distinguish shared and exclusive access, follow reborrows, and use graph
  reachability for liveness. Returned-reference summaries preserve all known possible roots.
- Object lifetime propagation uses a graph worklist. Storage availability remains separate from
  value lifetime so reconstruction can reuse relocated or destroyed storage. Loop backedges,
  exceptional construction, sequence escapes, and fallthrough retain their lifetime diagnostics.
- Stored reference fields, defaulted transfers, and bounded nested aggregates propagate known
  referents. Distinct known elements of direct local fixed arrays can be borrowed independently.
- Reference-bearing fixed arrays use separate holding-slot paths containing field and index
  projections. Definite covered writes end old contents; different or unknown indices preserve
  potentially surviving contents. Copying and returning holders uses reaching stores.
- Opaque reference calls conservatively retain known loans. Raw pointers do not create loans.
- Raw writes, placement operations, pointer publication, and opaque pointer calls can make
  tracked reference contents unknown. Safe extraction then requires established provenance or a
  definite covered replacement. The raw pointer operations themselves remain unrestricted.
- In-range writes into directly owned fixed arrays preserve independent scalar and sibling-field
  facts. An unproven index or raw-pointer write retains conservative memory invalidation.

Call contexts retain separate expression overrides and call bindings. They do not overwrite the
semantic body shared with ordinary callers. Already evaluated scalar arguments seed a callee's
analysis; only newly proven conditions or variant extractions justify retaining a context.
Equivalent overrides share a generated implementation. Indirect and virtual calls keep their
ordinary dispatch. Reference arguments require alias partitioning before they can seed contexts.

## Current precision and coverage limits

This is not yet a complete implementation of every planned verifier capability.

- Numeric function analysis currently skips graphs larger than 512 nodes. Loop widening and
  iteration limits discard unavailable precision rather than assuming convergence.
- Summaries containing loops, caught exceptions, arbitrary mutation through aliases, or
  unsupported operations remain opaque. In particular, the allocator and relocation paths in
  the current buffer implementation do not yet receive complete numeric summaries.
- Numeric field facts are not yet transferred through general aggregate relocation. Existing
  scalar relocation preserves evaluated value snapshots while ending access through the source.
- Variant summary payloads are restricted to operations whose effects are understood. General
  user-defined payload lifecycle effects remain conservative.
- Call-context analysis has bounded attempts, recursion depth, and implementation counts. A
  budget limit keeps the ordinary implementation and its runtime checks. Reference-field
  contexts and general recursive numeric summaries remain future work.
- Known reference stores through ordinary helper bodies propagate destination slots separately
  from referents. Lifetime checks use shared graph events; supported definite overwrites end
  those store paths only on normal completion. Opaque captures are retained through destruction
  and inferred across wrappers and recursive call groups. Arbitrary stores and unsupported
  aggregate provenance remain conservative analysis boundaries. Unknown foreign or raw-pointer
  provenance is not converted into a trusted tracked referent.
- Reference provenance expansion and special-member capture traversal have explicit limits.
  Exhaustion produces a diagnostic instead of silently discarding known reference roots.
- Holding paths are bounded to 16 projections. Returned aggregate slots can become wildcard
  paths when their identity is not unique, preserving roots while losing disjointness precision.
- Raw memory analysis does not discover arbitrary pointer referents. Unknown effects retain
  possible old roots and prevent treating those roots as complete reference provenance.
- Virtual and indirect reference calls conservatively permit capture. This can reject local
  reference uses even when the runtime target does not retain them.
- Lifetime reachability currently uses existing constant conditions rather than the full numeric
  relation state. Numeric facts primarily serve access diagnostics and check elimination.
- Constructor captures from unnamed temporaries or union subobjects still need enclosing-storage
  provenance; named owner capture coverage does not establish those cases.

## Verification

Run these with the built compiler, using `--compiler` to choose another staged binary:

```text
python tests/compiler-new/run_inferred_facts.py
python tests/compiler-new/run_function_facts.py
python tests/compiler-new/run_call_contexts.py
python tests/compiler-new/run_condition_effects.py
python tests/compiler-new/run_graph_lifetimes.py
python tests/compiler-new/run_reference_loans.py --compiler build/release/native/rutilec.exe
```

The function-fact and numeric suites inspect native disassembly at optimization zero and execute
boundary and side-effect cases. The diagnostic suite runner includes these six suites.
Self-hosting must also pass before committing an implementation batch.
