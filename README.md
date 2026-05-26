# Flang Implicit Allocation Profiler and Optimizer

Assignment 41 backend compiler project.

FIAP is a real end-to-end Flang pipeline:

```text
Fortran .f90 source -> Flang HLFIR/MLIR -> fiap-opt APG analysis -> JSON/CSV reports -> transform/evaluation
```

The main demo does **not** depend on handcrafted MLIR. The repository still keeps small MLIR regression fixtures for internal compiler-pass testing, but the submission workflow starts from real Fortran files in `testcases/fortran/`.

## What It Detects

FIAP finds hidden allocation sites introduced by Fortran lowering:

- array expression temporaries, for example `a = b + c`
- elemental array temporaries
- array-valued function results
- allocatable assignment/reallocation
- escaping temporaries that must not be optimized locally

Every report includes:

- source file, line, and column
- HLFIR/FIR operation name
- construct type
- estimated allocation bytes
- classification: `provably-eliminable`, `possibly-unnecessary`, or `necessary`
- reason and transformation advice

## Required Repository Contents

- `README.md` - what the project is and how to run it
- `DESIGN.md` - approach, alternatives, APG, classifier, failure handling
- `IMPLEMENTATION.md` - LLVM/MLIR/Flang implementation details
- `EVALUATION.md` - metrics, five Fortran test cases, baseline comparison, results
- `scripts/build.sh` and `scripts/run.sh` - required scripts
- `scripts/build.ps1` and `scripts/run.ps1` - Windows equivalents
- `src/` - source-level transformation helper
- `tools/` - `fiap-opt` driver
- `lib/` and `include/` - C++ analysis/pass implementation
- `testcases/fortran/` - real Fortran inputs for the main end-to-end pipeline
- `testcases/fortran_optimized/` - baseline comparison programs
- `test/mlir_regression/` - optional internal MLIR pass fixtures, not the main demo
- `profiles/` - profile-guided refinement sample data

## Requirements

Required:

- CMake 3.24 or newer
- C++17 compiler
- LLVM build with `LLVMConfig.cmake`
- MLIR build with `MLIRConfig.cmake`
- Flang executable for full end-to-end mode
- Python 3.10 or newer

Optional:

- `FlangConfig.cmake` for typed FIR/HLFIR C++ integration
- `ctest` for internal MLIR regression tests

## Build

Linux/macOS/WSL:

```bash
./scripts/build.sh
```

Windows PowerShell:

```powershell
scripts\build.ps1
```

Explicit Windows LLVM paths:

```powershell
scripts\build.ps1 `
  -LLVM_DIR D:\llvm-project\build\lib\cmake\llvm `
  -MLIR_DIR D:\llvm-project\build\lib\cmake\mlir `
  -Flang_DIR D:\llvm-project\build\lib\cmake\flang
```

## Run Full End-To-End Demo

This is the main command to show:

```bash
./scripts/run.sh
```

Windows:

```powershell
scripts\run.ps1
```

The default run performs the real pipeline:

1. Reads five `.f90` files from `testcases/fortran/`.
2. Uses Flang to emit HLFIR/MLIR into `reports/hlfir/*.mlir`.
3. Runs `build/fiap-opt.exe` over that generated HLFIR.
4. Writes FIAP JSON reports into `reports/hlfir/*.json`.
5. Writes `reports/hlfir/summary.csv` with strict expected-classification checks.
6. Rewrites the safe `vector_add.f90` case into `reports/source/vector_add.transformed.f90`.
7. Refines the real `function_result.f90` report using `profiles/sample_profile.csv`.
8. Compiles and times original vs optimized Fortran programs into `reports/benchmark/runtime.csv`.

## Five Real Fortran Test Cases

| Source file | Construct | Expected primary classification |
| --- | --- | --- |
| `testcases/fortran/vector_add.f90` | rank-1 array expression | `provably-eliminable` |
| `testcases/fortran/matrix_stencil.f90` | 2D elemental temporary | `provably-eliminable` |
| `testcases/fortran/function_result.f90` | array-valued function result | `possibly-unnecessary` |
| `testcases/fortran/allocatable_update.f90` | allocatable assignment/reallocation | `possibly-unnecessary` |
| `testcases/fortran/escaping_temp.f90` | escaping temporary passed to call | `necessary` |

The failure case is `escaping_temp.f90`: FIAP marks it `necessary` and does not suggest a local transform.

## Run One Generated HLFIR Report

After `scripts\run.ps1`, inspect a generated HLFIR report directly:

```powershell
build\fiap-opt.exe reports\hlfir\vector_add.mlir --format=text
build\fiap-opt.exe reports\hlfir\escaping_temp.mlir --format=text
```

The second command is the failure case.

## Real Fortran To HLFIR Command

You can also run only the Fortran-to-HLFIR analysis:

```powershell
python scripts\analyze_fortran_hlfir.py `
  --flang D:\llvm-project\build\bin\flang.exe `
  --tool build\fiap-opt.exe `
  --source-dir testcases\fortran `
  --out-dir reports\hlfir `
  --summary reports\hlfir\summary.csv `
  --strict
```

## Source Transformation Demo

The transform consumes the generated real-HLFIR report, not a mock report:

```powershell
python src\fiap_source_transformer.py `
  --report reports\hlfir\vector_add.json `
  --source testcases\fortran\vector_add.f90 `
  --output reports\source\vector_add.transformed.f90
```

It rewrites:

```fortran
a = b + c
```

into:

```fortran
do concurrent (i = lbound(a, 1):ubound(a, 1))
  a(i) = b(i) + c(i)
end do
```

## Optional Internal MLIR Regression

The `.mlir` files in `test/mlir_regression/*.mlir` are not the main demo. They are internal regression fixtures for checking the standalone MLIR pass deterministically.

Run them only if asked:

```powershell
scripts\run.ps1 -IncludeMlirRegression -IncludeCTest
```

## Current Status

Implemented:

- standalone C++ MLIR tool `fiap-opt`
- real `.f90 -> Flang HLFIR -> FIAP` workflow
- Allocation Provenance Graph construction
- conservative allocation classification
- source-location mapping
- byte estimation for static HLFIR/FIR array types
- JSON/text/DOT reporting
- IR annotation with `fiap.*` metadata
- source rewrite for simple rank-1 array expressions
- profile-guided refinement
- five real Fortran test cases
- baseline-vs-optimized Fortran benchmark harness

Future work:

- full in-place FIR/HLFIR mutation for every transform kind
- deeper interprocedural shape propagation
- larger benchmark suites such as LAPACK or SPEC CPU
