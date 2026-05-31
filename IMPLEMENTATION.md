# IMPLEMENTATION

## Build Target

The main executable is:

```text
build/fiap-opt
```

It is built by CMake from:

- `tools/fiap-opt.cpp`
- `lib/*.cpp`
- `lib/Transforms/*.cpp`

The required libraries are LLVM and MLIR. Flang is optional at build time.

## LLVM/MLIR Driver

`tools/fiap-opt.cpp` is a standalone MLIR tool. It:

1. parses an MLIR input file
2. registers project dialect support
3. allows unregistered dialects so FIR/HLFIR textual IR can be tested portably
4. runs `ImplicitAllocationProfilerPass`
5. optionally runs safe transform passes
6. emits text, JSON, DOT, profile-site CSV, or post-transform annotated IR output

Important command-line options:

```text
--format=text
--format=json
--format=dot
--format=profile-sites
--format=sarif
--emit-profile-sites
--prepare-transforms
--apply-transforms
--print-annotated-ir
--include-non-allocation-nodes
```

SARIF output uses rule `FIAP001` and carries the same classification, byte estimate, source location, source snippet, best-effort RHS expression range, rank, shape extent spelling, legality, alias evidence, shape evidence, reason, and transform advice as the JSON report. It is intended for code-scanning tools and review artifacts.

`fiap::registerFIAPPasses()` also registers the reusable MLIR pipeline:

```text
fiap-profile-and-transform
```

This gives an upstream Flang integration point in addition to the standalone `fiap-opt` driver.

The CMake project also installs an exported package target:

```text
FIAP::fiap
```

so an external Flang checkout or downstream MLIR tool can link the analysis and transform passes without copying source files.

## Main C++ Components

### APG

Files:

- `include/fiap/APG.h`
- `lib/APG.cpp`

Defines the graph node and edge structures used by the analysis. Nodes preserve source location, operation kind, shape estimate, classification, and transformation advice.

### Operation Semantics

Files:

- `include/fiap/OperationSemantics.h`
- `lib/OperationSemantics.cpp`

Recognizes allocation-relevant FIR/HLFIR operations.

Generic mode checks operation names such as:

- `hlfir.elemental`
- `hlfir.as_expr`
- `hlfir.associate`
- `hlfir.assign`
- `hlfir.destroy`
- `fir.allocmem`
- `fir.freemem`
- `fir.call`
- `func.call`

Typed mode is enabled when `FlangConfig.cmake` is found. In that case the project can register upstream FIR/HLFIR dialect components where the local Flang build exposes them.

### Implicit Allocation Analysis

Files:

- `include/fiap/ImplicitAllocationAnalysis.h`
- `lib/ImplicitAllocationAnalysis.cpp`

Walks the MLIR module, builds APG nodes, extracts source locations, estimates bytes from type spellings, and records loop/escape/consumer context.

Static type parsing handles patterns such as:

```text
!hlfir.expr<1024xf32>
!fir.array<1024xf64>
!fir.box<!fir.array<?xf64>>
```

Dynamic shape information is marked runtime-dependent so the classifier can avoid unsafe elimination. When a dynamic temporary feeds an assignment with a statically known destination shape on the same source line, FIAP records assignment-compatible shape evidence and carries that byte estimate into the report.

### Classifier

Files:

- `include/fiap/AllocationClassifier.h`
- `lib/AllocationClassifier.cpp`

Assigns one of:

- `provably-eliminable`
- `possibly-unnecessary`
- `necessary`

It also records:

- reason
- advice
- suggested transform
- transformability flag

### Profiler Pass

Files:

- `include/fiap/Passes.h`
- `lib/ImplicitAllocationProfilerPass.cpp`

Connects analysis, classification, reporting, and optional IR annotation.

The pass attaches metadata such as:

```text
fiap.classification
fiap.construct
fiap.estimated_bytes
fiap.reason
fiap.transform
fiap.advice
```

### Reporting

Files:

- `include/fiap/AllocationReport.h`
- `lib/AllocationReport.cpp`

Supported outputs:

- text report for terminal demo
- JSON report for scripts and evaluation
- DOT graph for APG visualization
- profile-site CSV for profile-guided refinement

Profile-site CSV rows include the stable site key plus source expression, rank, shape extents, element byte width, estimated element count, observed bytes, allocation count, and `shape-and-allocation-counter` instrumentation kind. This keeps the refinement step tied to source-level evidence instead of only file/line pairs.

The JSON schema has:

```json
{
  "summary": {
    "totalSites": 0,
    "provablyEliminable": 0,
    "possiblyUnnecessary": 0,
    "necessary": 0,
    "totalEstimatedBytes": 0
  },
  "entries": []
}
```

### HLFIR/FIR Transform Passes

Files:

- `lib/Transforms/PromoteTempToStack.cpp`
- `lib/Transforms/ScalarizeArrayExpr.cpp`

When FIAP is built with Flang headers/libraries, these passes perform real typed FIR/HLFIR mutation.

`ScalarizeArrayExpr.cpp` handles safe `hlfir.elemental` expressions that feed exactly one `hlfir.assign`, are marked `legal-for-rewrite`, and do not carry conservative alias evidence. It uses Flang's own HLFIR builder helpers to:

1. generate a `fir.do_loop` nest over the elemental shape, including higher-rank arrays such as the rank-3 tensor testcase
2. inline the elemental body at each loop index
3. write the computed scalar directly into the destination element
4. erase the original `hlfir.elemental`, `hlfir.assign`, and `hlfir.destroy`

Nested elemental expressions are recursively inlined when they are also FIAP scalarization candidates. This lets `matrix_stencil.f90`, `laplace2d_real_kernel.f90`, `saxpy_real_kernel.f90`, `option_pricing_real_kernel.f90`, and `polybench_jacobi1d.f90` remove nested elemental temporaries.

`PromoteTempToStack.cpp` performs guarded `fir.allocmem -> fir.alloca` promotion. It only rewrites allocations already classified as `provably-eliminable` with `promote-to-stack` and `legal-for-rewrite`, and skips true heap-required storage, loop-local dynamic allocations, and dynamic shape/length allocations. The replacement uses `fir.alloca` plus a type-compatible `fir.convert` bridge and removes direct `fir.freemem` users.

Without Flang typed support, these passes fall back to metadata-only annotation so the analysis tool still builds in generic MLIR environments.

## Source-Level Transformer

File:

- `src/fiap_source_transformer.py`

This is the concrete auto-transformation required for the project. It consumes a FIAP JSON report and rewrites simple provably eliminable rank-1 through rank-15 array expressions. It also rewrites allocatable assignment cases into explicit allocation guards and converts simple array-valued function results into subroutines with explicit result buffers.

Example input:

```fortran
a = b + c
```

Output:

```fortran
do concurrent (i = lbound(a, 1):ubound(a, 1))
  a(i) = b(i) + c(i)
end do
```

Safety checks:

- report entry must be `provably-eliminable`
- transform must be `scalarize-to-loop-nest`
- source file must match the report
- source line must be a simple assignment
- rank must be 1 through 15 for array-expression scalarization
- allocatable shape guards require matching allocatable ranks
- function-result conversion requires a simple `lhs = function(args...)` call and a local `function ... result(out)` definition
- ambiguous lines are skipped

## Scripts

Required scripts:

- `build.sh` - configure and build
- `run.sh` - end-to-end Fortran pipeline or single generated-HLFIR report
- `package_release.sh` - create a source release archive
- `scripts/build.sh`, `scripts/run.sh`, and `scripts/package_release.sh` - compatibility wrappers for the old script paths

Backend workflow script:

- `scripts/run_pipeline.py`

Evaluation helpers:

- `scripts/analyze_fortran_hlfir.py`
- `scripts/generate_profile.py`
- `scripts/refine_profile.py`
- `scripts/benchmark_fortran.py`
- `scripts/check_pipeline_outputs.py`
- `scripts/run_lit_style_checks.py`

## Real Flang HLFIR Path

`scripts/analyze_fortran_hlfir.py` runs:

```text
flang -fc1 -emit-hlfir -mmlir --mlir-print-debuginfo
```

Then it passes the generated `.mlir` file into `fiap-opt`.

This is the default submission path. The required and extended real-kernel test cases are real `.f90` files that Flang lowers into HLFIR before FIAP analyzes them.

CTest runs the same path through `fiap_real_fortran_pipeline`, verifies generated reports through `fiap_pipeline_evidence_checks`, and runs generated-evidence regression assertions through `fiap_lit_style_regression_checks`. Those checks validate source-expression precision, unsafe-case blocking, backend rewrite evidence, profile refinement, and benchmark output equivalence.
