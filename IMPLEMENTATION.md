# IMPLEMENTATION

## Build Target

The main executable is:

```text
build/fiap-opt.exe
```

or on Unix-like systems:

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
5. optionally runs transform-preparation passes
6. emits text, JSON, DOT, or annotated IR output

Important command-line options:

```text
--format=text
--format=json
--format=dot
--prepare-transforms
--print-annotated-ir
--include-non-allocation-nodes
```

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

Dynamic shape information is marked runtime-dependent so the classifier can avoid unsafe elimination.

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

### Transform Preparation

Files:

- `lib/Transforms/PromoteTempToStack.cpp`
- `lib/Transforms/ScalarizeArrayExpr.cpp`

These passes prepare transform hints and annotations. They are intentionally conservative because full FIR/HLFIR mutation is sensitive to exact Flang revision details.

## Source-Level Transformer

File:

- `src/fiap_source_transformer.py`

This is the concrete auto-transformation required for the project. It consumes a FIAP JSON report and rewrites simple provably eliminable rank-1 array expressions.

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
- ambiguous lines are skipped with an explanation

## Scripts

Required scripts:

- `scripts/build.sh` - configure and build
- `scripts/run.sh` - end-to-end Fortran pipeline or single generated-HLFIR report

Windows equivalents:

- `scripts/build.ps1`
- `scripts/run.ps1`

Backend workflow script:

- `scripts/run_pipeline.py`

Evaluation helpers:

- `scripts/analyze_fortran_hlfir.py`
- `scripts/refine_profile.py`
- `scripts/benchmark_fortran.py`

## Real Flang HLFIR Path

`scripts/analyze_fortran_hlfir.py` runs:

```text
flang -fc1 -emit-hlfir -mmlir --mlir-print-debuginfo
```

Then it passes the generated `.mlir` file into `fiap-opt`.

This is the default submission path. The five required test cases are real `.f90` files that Flang lowers into HLFIR before FIAP analyzes them.
