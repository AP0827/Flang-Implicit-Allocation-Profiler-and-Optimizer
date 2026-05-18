# IMPLEMENTATION

## LLVM/MLIR Integration

The project builds a standalone `fiap-opt` tool on top of LLVM and MLIR. It parses an MLIR module, registers project dialect support, allows unregistered FIR/HLFIR operations for generic mode, and runs a pass pipeline.

Main entry points:

- `tools/fiap-opt.cpp`: command-line driver
- `lib/ImplicitAllocationProfilerPass.cpp`: pass that builds the APG, classifies nodes, annotates IR, and emits reports
- `lib/ImplicitAllocationAnalysis.cpp`: module walk, graph construction, source mapping, loop-depth extraction
- `lib/OperationSemantics.cpp`: generic and optional typed FIR/HLFIR operation matching
- `lib/AllocationClassifier.cpp`: eliminability lattice and rewrite advice
- `lib/AllocationReport.cpp`: text, JSON, and DOT output
- `lib/Transforms/*.cpp`: transformation-preparation passes

## Operation Detection

Generic mode recognizes FIR/HLFIR operations by operation name and type spelling:

- `hlfir.as_expr` -> array expression temporary
- `hlfir.elemental` -> elemental temporary
- `hlfir.associate` -> temporary lifetime extension
- `fir.allocmem` -> heap allocation
- `hlfir.assign` with `realloc` attribute -> realloc-on-assignment
- `fir.call` / `func.call` returning array-like type -> function-result temporary
- `hlfir.destroy` / `fir.freemem` -> lifetime end

When Flang headers are available, `FIAP_HAVE_FLANG` enables typed matching through FIR/HLFIR operation classes and dialect registration.

## Source Mapping

The pass extracts `FileLineColLoc`, unwraps `CallSiteLoc`, `NameLoc`, and `FusedLoc`, and stores source anchors on APG nodes. Reports therefore point back to Fortran locations such as:

```text
matvec_temp.f90:11:9: [provably-eliminable] array expression may materialize a temporary
```

## Byte Estimation

The analysis parses FIR/HLFIR array type spellings such as:

```text
!hlfir.expr<1024xf32>
!fir.box<!fir.array<?xf64>>
```

Static shapes produce estimated bytes. Dynamic shapes are marked runtime-dependent and classified conservatively.

## Reports

`fiap-opt` supports:

- text report: human-readable demo output
- JSON report: evaluation and script input
- DOT report: APG graph visualization
- annotated IR: input IR with `fiap.*` attributes

Common commands:

```bash
./build/fiap-opt testcases/01_array_temp.mlir
./build/fiap-opt testcases/01_array_temp.mlir --format=json
./build/fiap-opt testcases/01_array_temp.mlir --format=dot
./build/fiap-opt testcases/01_array_temp.mlir --prepare-transforms --print-annotated-ir
```

## Source Transformation

`src/fiap_source_transformer.py` implements the simple required auto-transformation. It consumes a JSON report and a Fortran source file. For a provably eliminable rank-1 array assignment, it rewrites:

```fortran
a = b + c
```

into:

```fortran
do concurrent (i = lbound(a, 1):ubound(a, 1))
  a(i) = b(i) + c(i)
end do
```

The transformer refuses unclear or unsafe lines, which is the correct behavior for the failure-case demo.

## Profile-Guided Refinement

`scripts/refine_profile.py` implements the PGAE prototype from the research plan. It consumes:

- a FIAP JSON report
- a profile CSV keyed by `file,line,column`

If a profile row proves that a `possibly-unnecessary` site had a stable shape during all observations, the script upgrades that site to `provably-eliminable`, records the profile evidence, and keeps or assigns an appropriate guarded transform. This gives a concrete two-pass workflow:

1. Static pass: classify sites and emit JSON.
2. Profile refinement: upgrade ambiguous sites using runtime evidence.

The sample profile is `profiles/sample_profile.csv`.

## Runtime Benchmarking

`scripts/benchmark_fortran.py` compiles and times matching baseline/optimized programs from:

- `testcases/fortran/`
- `testcases/fortran_optimized/`

It writes `reports/runtime-benchmark.csv`. The script discovers `flang-new`, `flang`, `gfortran`, or `ifx`; if none are installed, it writes an explicit skipped result instead of inventing speedup numbers.

## Build-Time Options

Required:

- CMake >= 3.24
- C++17 compiler
- LLVMConfig.cmake
- MLIRConfig.cmake

Optional:

- FlangConfig.cmake for typed FIR/HLFIR matching

Environment variables used by scripts:

- `LLVM_DIR`
- `MLIR_DIR`
- `Flang_DIR`
- `BUILD_DIR`
