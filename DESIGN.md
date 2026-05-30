# DESIGN

## Problem

Fortran developers often write array syntax that appears stack-cheap:

```fortran
A = B + C
```

During lowering, Flang may materialize implicit temporaries for array expressions, elemental expressions, function results, or allocatable assignment. These allocations are not obvious in source and can dominate runtime inside loops.

FIAP is designed to expose those allocations at the HLFIR/FIR level, map them back to source, classify whether they are removable, and provide transformation advice.

## Design Goals

- Start from compiler IR evidence, not source guesses.
- Work with real Flang-generated HLFIR when Flang is available.
- Use real Flang-generated HLFIR as the default evaluation path.
- Produce source-mapped reports suitable for a GitHub/demo submission.
- Include a conservative failure case where the tool refuses unsafe transformation.
- Provide measurable evaluation through repeatable scripts and CSV reports.

## Allocation Provenance Graph

The central design is an Allocation Provenance Graph, or APG.

Each relevant operation becomes a node:

- `hlfir.elemental`
- `hlfir.as_expr`
- `hlfir.associate`
- `hlfir.assign`
- `fir.allocmem`
- `fir.call` or `func.call`
- `hlfir.destroy`
- `fir.freemem`

Each node records:

- operation name
- source file, line, and column
- construct kind
- estimated bytes
- static or runtime-dependent shape
- loop depth
- producer count
- consumer count
- escape status
- suggested transform

Edges describe why one operation matters to another:

- `produces` - an operation creates a temporary value
- `consumes` - assignment/call/destruction consumes it
- `shape-constrains` - nearby shape information bounds the temporary
- `aliases` - multiple values may observe the same allocation
- `lifetime-ends` - a destroy/free operation ends the temporary lifetime

This graph lets the report explain not only that an allocation exists, but why it exists and why a transformation is or is not safe.

## Classification Lattice

FIAP classifies allocation sites into a three-level conservative lattice:

1. `provably-eliminable`

   The allocation is local, non-escaping, shape-bounded, and single-consumer. Examples include simple array expressions that can be scalarized into a loop or small bounded temporaries that can be stack-promoted.

2. `possibly-unnecessary`

   The allocation may be removable, but static proof is incomplete. Examples include array-valued function results or allocatable assignment where runtime shape equality needs confirmation.

3. `necessary`

   Local elimination is unsafe. The main example is a temporary that escapes through a call or has multiple unknown consumers.

The analysis is intentionally conservative. It is better to report an uncertain site than to silently rewrite unsafe code.

## Transformation Strategy

The backend has two transformation levels.

First, the C++ pass annotates IR with `fiap.*` metadata and transform hints:

- `scalarize-to-loop-nest`
- `promote-to-stack`
- `preallocate-lhs`
- `add-shape-guard`
- `none`

Second, `src/fiap_source_transformer.py` implements the required source-level transformation for the simplest safe case. It rewrites a provably eliminable rank-1 assignment into a `do concurrent` loop.

The project does not overclaim full general-purpose Fortran rewriting. Complex FIR/HLFIR mutation is documented as future work, while the implemented simple transformation is concrete and demonstrable.

## Profile-Guided Refinement

Some sites are ambiguous because shape equality is runtime-dependent. FIAP includes a profile-guided refinement step:

1. Static FIAP report marks a site as `possibly-unnecessary`.
2. A profile CSV records observed allocation count, bytes, and shape stability.
3. `scripts/refine_profile.py` upgrades stable ambiguous sites to `provably-eliminable` under profile evidence.

This demonstrates the planned profile-guided allocation elimination loop without requiring a heavy runtime instrumentation framework.

## Alternatives Considered

### Source-Only Scanner

Rejected as the primary method. Source can show array syntax, but it cannot reliably prove whether Flang materializes heap allocation after lowering.

### Runtime Malloc Interposition

Useful for validation, but it does not explain which HLFIR operation caused the allocation or how to transform it.

### Fully Flang-Only Plugin

Ideal for production, but fragile for classroom machines because exact Flang headers and libraries may not be available. FIAP supports typed Flang integration when available and generic MLIR operation matching otherwise.

## Failure Handling

The failure case is deliberate and comes from real Fortran source:

- `testcases/fortran/escaping_temp.f90`
- generated HLFIR: `reports/hlfir/escaping_temp.mlir`
- classification: `necessary`
- reason: temporary escapes through an interprocedural call
- transform: `none`

This is important for the demo because it shows the optimizer is not blindly rewriting every allocation-like pattern.
