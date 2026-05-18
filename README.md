# Flang Implicit Allocation Profiler and Optimizer

`fiap` is a Flang/MLIR analysis and transformation-preparation tool for finding implicit heap allocations introduced during HLFIR/FIR lowering. It maps allocation sites back to source constructs, classifies their eliminability, emits reports, and prepares simple rewrites such as stack promotion and scalarization.

This version goes beyond a bare scaffold:

- it constructs an `Allocation Provenance Graph` (APG)
- it classifies each site with reasons and suggested rewrites
- it emits text, JSON, or DOT reports
- it annotates the IR with machine-readable `fiap.*` attributes
- it includes prototype transformation-preparation passes for stack promotion and scalarization
- it can run in a generic MLIR mode with unregistered FIR/HLFIR operations, which makes demos and testing much easier
- it can optionally compile with upstream Flang headers and use typed FIR/HLFIR op matching instead of string-only matching

## What The Tool Detects

- array expression temporaries such as `A = B + C`
- elemental temporaries
- array-valued function result temporaries
- allocatable assignment sites that may trigger realloc-on-assignment
- temporary lifetime extension through `hlfir.associate`

## Core Architecture

### Allocation Provenance Graph

Each interesting HLFIR/FIR operation becomes an APG node with:

- operation kind
- source location
- shape estimate
- estimated bytes
- loop depth
- escape status
- consumers and producers
- suggested transformation

Edges encode:

- `produces`
- `consumes`
- `shape-constrains`
- `aliases`
- `lifetime-ends`

### Classification Lattice

- `provably-eliminable`
- `possibly-unnecessary`
- `necessary`

The classifier also emits:

- a short reason
- an optimization hint
- a suggested transform kind

## Project Layout

- `DESIGN.md`: approach, alternatives, classification strategy, and failure cases
- `IMPLEMENTATION.md`: LLVM/MLIR details and pass pipeline
- `EVALUATION.md`: metrics, test matrix, and demo evidence checklist
- `include/fiap/`: public APIs
- `lib/`: APG construction, classification, reporting, and passes
- `src/fiap_source_transformer.py`: simple source-level transformer for rank-1 array assignments
- `tools/fiap-opt.cpp`: standalone driver
- `examples/`: generic MLIR and Fortran examples
- `test/`: FileCheck-style smoke-test inputs
- `testcases/`: required submission testcases, including three Fortran kernels and five MLIR cases
- `docs/`: architecture and research framing
- `docs/local-flang-build.md`: practical guide for a real LLVM/Flang build
- `benchmarks/`: evaluation notes and result templates
- `scripts/build.sh`, `scripts/run.sh`, `scripts/evaluate.py`: required build, demo, and evaluation scripts
- `scripts/refine_profile.py`: profile-guided allocation lattice refinement
- `scripts/benchmark_fortran.py`: runtime benchmark harness for baseline vs optimized Fortran programs
- `profiles/sample_profile.csv`: sample profile data for the PGAE demo

## Build

The project only requires LLVM + MLIR. Flang is optional for the generic prototype mode.

Preferred submission command:

```bash
./scripts/build.sh
```

On this Windows machine, `bash` is unavailable because WSL has no installed distro. Use the equivalent PowerShell script:

```powershell
scripts\build.ps1
```

Set these variables if your LLVM build is not in the default location:

```bash
export LLVM_DIR=/path/to/llvm/lib/cmake/llvm
export MLIR_DIR=/path/to/llvm/lib/cmake/mlir
export Flang_DIR=/path/to/llvm/lib/cmake/flang   # optional
```

```powershell
cmake -S . -B build `
  -DLLVM_DIR=D:\llvm-project\build\lib\cmake\llvm `
  -DMLIR_DIR=D:\llvm-project\build\lib\cmake\mlir

cmake --build build --config Release
```

If you have a full Flang build available, you can also point CMake at it:

```powershell
cmake -S . -B build `
  -DLLVM_DIR=D:\llvm-project\build\lib\cmake\llvm `
  -DMLIR_DIR=D:\llvm-project\build\lib\cmake\mlir `
  -DFlang_DIR=D:\llvm-project\build\lib\cmake\flang
```

When `Flang_DIR` is available, the project enables a typed integration path that:

- registers FIR and HLFIR dialects through upstream `InitFIR.h`
- uses real `fir::` and `hlfir::` op classes where possible
- falls back to generic MLIR operation matching only when needed

## Running

Preferred submission command:

```bash
./scripts/run.sh testcases/01_array_temp.mlir
```

Windows equivalent:

```powershell
scripts\run.ps1 testcases\01_array_temp.mlir
```

### Text Report

```powershell
build\fiap-opt.exe examples\implicit_temp.mlir
```

### JSON Report

```powershell
build\fiap-opt.exe examples\implicit_temp.mlir --format=json
```

### Graph Visualization

```powershell
build\fiap-opt.exe examples\implicit_temp.mlir --format=dot
```

### Annotated IR

```powershell
build\fiap-opt.exe examples\implicit_temp.mlir --prepare-transforms --print-annotated-ir
```

### Batch Collection

```powershell
scripts\collect-reports.ps1 -Format json
```

### Evaluation

```bash
python scripts/evaluate.py --tool build/fiap-opt --testcases testcases --out reports/evaluation-summary.csv
```

### Profile-Guided Refinement

```bash
build/fiap-opt.exe testcases/02_function_result.mlir --format=json > reports/function_result.json
python scripts/refine_profile.py \
  --report reports/function_result.json \
  --profile profiles/sample_profile.csv \
  --out reports/function_result.refined.json
```

### Runtime Benchmark Harness

If a Fortran compiler is installed, benchmark the baseline and optimized programs:

```bash
python scripts/benchmark_fortran.py --out reports/runtime-benchmark.csv
```

Supported compilers are discovered in this order: `flang-new`, `flang`, `gfortran`, `ifx`.

### Simple Source Rewrite Demo

Generate a JSON report:

```bash
FORMAT=json ./scripts/run.sh testcases/01_array_temp.mlir > reports/01_array_temp.json
```

Apply the simple source rewrite:

```bash
python src/fiap_source_transformer.py \
  --report reports/01_array_temp.json \
  --source testcases/fortran/vector_add.f90 \
  --output reports/vector_add.transformed.f90
```

## Output Style

The text report includes:

- source location
- classification
- construct kind
- estimated bytes
- loop depth
- producer and consumer counts
- reason for the classification
- advice for eliminating or validating the allocation

The JSON report is suitable for:

- dashboards
- benchmark aggregation
- paper figures
- profile-guided follow-up tooling

## Prototype Transformations

Two follow-on passes are included:

- `PromoteTempToStackPass`
- `ScalarizeArrayExprPass`

At this stage they attach lowering hints and rewrite templates rather than performing full FIR/HLFIR mutation. This keeps the prototype practical without requiring hard-coding against a single fragile Flang revision.

## Best Current Status

What is implemented now:

- APG construction with explicit node and edge types
- shape parsing from FIR/HLFIR type spellings
- source mapping
- conservative escape and alias heuristics
- classification with reasons and rewrite advice
- text, JSON, and DOT reports
- IR annotation with `fiap.*` metadata
- self-contained generic MLIR examples
- report collection scripting for experiments
- profile-guided refinement from runtime shape observations
- runtime benchmark harness for the three Fortran kernels when a compiler is available
- optional typed Flang integration layer built around official FIR/HLFIR headers

What still remains future work:

- exact dialect-aware rewrites using FIR/HLFIR builders
- interprocedural shape propagation
- full benchmark automation against large external suites such as LAPACK or SPEC CPU
