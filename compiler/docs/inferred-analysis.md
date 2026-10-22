# Inferred function facts and reference analysis

The compiler infers these facts from resolved function bodies. There is no public proof syntax.
The implementation is staged; this document distinguishes the working foundation from remaining
analysis work.

## Analysis pipeline

`InferredFacts` runs after semantic construction and before the existing lifetime analysis.
`ReferenceLoanAnalyzer` then checks tracked reference accesses. Both use `FactGraph`; the existing
lifetime and frontend variant analyses remain in place.

The graph represents branches, short circuiting, loops, switches, returns, throws, and handlers.
Exception dispatch deliberately includes every potentially matching handler. Unsupported effects
lose information instead of establishing assumptions.

Places identify object roots and field projections. Numeric state versions distinguish values
before and after writes. The abstract domain includes integer intervals, exact negative values,
equalities and comparisons, overflow-checked affine offsets, and active variant alternatives.
Queries distinguish proven, disproven, and unknown conditions.

Function summaries contain symbolic entry-state expressions, path conditions, returned values,
and writes. Boolean and union outcomes retain their correlations with resulting state. Only facts
common to every corresponding return path are exported. Read-only wrappers compose summaries to
a fixed point; unresolved recursion and dynamically dispatched calls remain conservative.

A direct throw ends its normal path. A scalar memory read can return an unknown value while
preserving the guards on that path. A read-only wrapper imports its callee's single normal
outcome, including those guards. This does not classify a throw as a compile-time violation.

Opaque calls, pointer writes, user special members without usable summaries, and uncertain alias
effects invalidate potentially affected facts. No method names identify getters or mutators.

## Observable behavior

- Provably invalid fixed-array accesses produce a compile-time diagnostic, including negative
  indices. Uncertain accesses retain their existing runtime behavior.
- Proven scalar expressions become constants in native generation when evaluation has no
  observable effects. Checked variant extraction also consumes proven alternatives.
- Repeated guards and relations learned across calls can disappear at optimization level zero.
- Guarded growth can establish that a new value exceeds its previous value. Unguarded wrapping
  arithmetic does not establish that relationship.
- Local reference loans distinguish shared and exclusive access, follow reborrows, and use graph
  reachability for liveness. Returned-reference summaries preserve all known possible roots.
- Stored reference fields, defaulted transfers, and bounded nested aggregates propagate known
  referents. Distinct known elements of direct local fixed arrays can be borrowed independently.
- Opaque reference calls conservatively retain known loans. Raw pointers remain untracked.

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
- Variant summary payloads are restricted to operations whose effects are understood. General
  user-defined payload lifecycle effects remain conservative.
- Call-context analysis has bounded attempts, recursion depth, and implementation counts. A
  budget limit keeps the ordinary implementation and its runtime checks. Reference-field
  contexts and general recursive numeric summaries remain future work.
- Known reference stores through ordinary helper bodies propagate destination slots separately
  from referents. Lexical lifetime checks detect known local-to-outer stores; supported definite
  overwrites end those store paths. Arbitrary stores and complete validation of escaping loans
  at implicit lifetime boundaries remain incomplete. Unknown foreign or raw-pointer provenance
  is not converted into a trusted tracked referent.
- The existing lifetime pass has not yet been migrated onto the shared graph.

## Verification

Run these with the built compiler, using `--compiler` to choose another staged binary:

```text
python tests/compiler-new/run_inferred_facts.py
python tests/compiler-new/run_function_facts.py
python tests/compiler-new/run_call_contexts.py
python tests/compiler-new/run_reference_loans.py --compiler build/release/native/rutilec.exe
```

The function-fact and numeric suites inspect native disassembly at optimization zero and execute
boundary and side-effect cases. The diagnostic suite runner includes these four suites.
Self-hosting must also pass before committing an implementation batch.
