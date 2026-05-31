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
- assignment-compatible shape evidence when a dynamic temporary is bounded by the destination assignment
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

Each allocation-bearing node also carries a legality decision. The current legality states are intentionally explicit: `legal-for-rewrite`, `needs-runtime-guard`, `needs-profile-evidence`, `needs-interprocedural-rewrite`, `illegal-for-local-rewrite`, and `unproven`. Pointer-like, descriptor-like, class-like, target, and nested alias-sensitive operations add conservative alias evidence that blocks local rewriting.

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

The backend has three transformation levels.

First, the profiler pass annotates IR with `fiap.*` metadata and transform hints:

- `scalarize-to-loop-nest`
- `promote-to-stack`
- `preallocate-lhs`
- `add-shape-guard`
- `none`

Second, typed C++ transform passes apply safe FIR/HLFIR rewrites when Flang support is available. The implemented in-place IR rewrite scalarizes provably eliminable, alias-clean, `legal-for-rewrite` `hlfir.elemental` assignments into explicit `fir.do_loop` nests and recursively inlines nested elemental computations, including higher-rank cases. A guarded stack-promotion rewrite converts bounded temporary `fir.allocmem` sites into `fir.alloca` and refuses true heap-required or dynamic allocatable storage.

Third, `src/fiap_source_transformer.py` implements concrete source-level transformations for safe cases. It rewrites provably eliminable rank-1 through rank-15 array assignments into explicit loop nests, rewrites allocatable assignments into explicit allocation/shape guards, and converts simple array-valued function results into subroutines with explicit result buffers.

The transform contract is intentionally explicit: only sites marked `legal-for-rewrite` by the classifier are rewritten. Unsafe pointer, descriptor, class, target, escaping, and strided-section cases remain in report-only mode with the blocking evidence recorded in JSON/SARIF. `fiap-opt --apply-transforms --print-annotated-ir` prints the post-transform IR so the generated `reports/hlfir/*.transformed.mlir` files show exactly which HLFIR/FIR operations were rewritten.

## Profile-Guided Refinement

Some sites are ambiguous because shape equality is runtime-dependent. FIAP includes a profile-guided refinement step:

1. Static FIAP report marks a site as `possibly-unnecessary`.
2. FIAP emits profile-site data from reports into `reports/profile/generated_profile.csv`.
3. `scripts/refine_profile.py` upgrades stable ambiguous sites to `provably-eliminable` under profile evidence.

This implements the profile-guided refinement loop at report level. The generated profile-site data includes site IDs, source expression, rank, shape extents, element width, observed shape stability, observed bytes, allocation count, and instrumentation kind so ambiguous sites are refined from structured evidence rather than ad hoc manual notes.

## Alternatives Considered

### Source-Only Scanner

Rejected as the primary method. Source can show array syntax, but it cannot reliably prove whether Flang materializes heap allocation after lowering.

### Runtime Malloc Interposition

Useful for validation, but it does not explain which HLFIR operation caused the allocation or how to transform it.

### Fully Flang-Only Plugin

Ideal for production, but fragile for classroom machines because exact Flang headers and libraries may not be available. FIAP supports typed Flang integration when available and generic MLIR operation matching otherwise.

The repository includes `upstream/flang-fiap-integration.patch` as an integration kit. It is intentionally kept outside the default scripts because it targets an external LLVM checkout, but it documents the concrete pass-registration and pipeline-insertion points.

## Failure Handling

The failure case is deliberate and comes from real Fortran source:

- `testcases/fortran/escaping_temp.f90`
- generated HLFIR: `reports/hlfir/escaping_temp.mlir`
- classification: `necessary`
- reason: temporary escapes through an interprocedural call
- transform: `none`

This is important for the demo because it shows the optimizer is not blindly rewriting every allocation-like pattern.
