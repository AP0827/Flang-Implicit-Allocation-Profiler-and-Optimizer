# Research Plan

## Publishable Thesis

HLFIR makes compiler-inserted Fortran array temporaries visible for the first time in a modern MLIR pipeline. `fiap` turns that visibility into a static analysis and transformation framework for implicit memory allocation.

## Contributions

1. An APG representation for allocation provenance in HLFIR/FIR.
2. A three-point lattice for eliminability classification.
3. A source-mapped reporting pass for hidden allocations.
4. A structured JSON and DOT reporting format for experimental analysis.
5. Typed HLFIR/FIR transformation passes for guarded stack promotion and scalarization.
6. Profile-guided refinement for ambiguous allocation sites using structured profile-site evidence.

## Evaluation Plan

### Metrics

- number of implicit allocations
- total allocated bytes
- peak live temporary bytes
- end-to-end runtime
- compile-time overhead of the pass

### Candidate workloads

- the thirteen real Fortran inputs in `testcases/fortran/`
- selected numerical kernels for SAXPY, Laplace 2D, option pricing, rank-3 tensor update, and PolyBench-style Jacobi 1D
- negative cases for escaping temporaries, pointer aliases, assumed-shape descriptors, and strided sections
- external benchmark suites such as SPEC CPU or LAPACK when available locally

### Experimental comparisons

- baseline Flang lowering
- reporting-only analysis
- stack-promotion enabled
- scalarization enabled
- profile-guided reclassification enabled

## Completed Milestones

1. Detect and report hidden allocation sites.
2. Attach source ranges and estimated byte counts.
3. Classify each site using conservative rules.
4. Implement guarded automatic rewrites at source and HLFIR/FIR level.
5. Evaluate on thirteen real Fortran programs with baseline comparison and failure cases.

## Practical Publishable Angle

The most defensible paper angle for this repository is:

`A provenance-driven static analysis for implicit allocation in MLIR-based Fortran compilation, with structured evidence for later transformation and profiling.`

That is stronger than presenting the work as a simple warning pass because it turns the implementation into:

- an analysis abstraction
- a classification framework
- a transformation staging mechanism
