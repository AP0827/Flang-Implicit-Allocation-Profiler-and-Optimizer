# Flang Implicit Allocation Profiler and Optimizer

Assignment 41 backend compiler project.

FIAP is a real end-to-end Flang pipeline:

```text
Fortran .f90 source -> Flang HLFIR/MLIR -> fiap-opt APG analysis -> HLFIR/FIR rewrites + source rewrites -> evaluation
```

The project workflow starts from real Fortran files in `testcases/fortran/`. There are no handcrafted MLIR fixtures in the submission testcases.

## What It Detects

FIAP finds hidden allocation sites introduced by Fortran lowering:

- array expression temporaries, for example `a = b + c`
- elemental array temporaries
- array-valued function results
- allocatable assignment/reallocation
- escaping temporaries that must not be optimized locally

Every report includes:

- source file, line, and column
- source line and best-effort RHS expression snippet
- HLFIR/FIR operation name
- construct type
- rank, shape extent spelling, element count, and element byte width
- estimated allocation bytes
- classification: `provably-eliminable`, `possibly-unnecessary`, or `necessary`
- reason and transformation advice

## Required Repository Contents

- `README.md` - what the project is and how to run it
- `DESIGN.md` - approach, alternatives, APG, classifier, failure handling
- `IMPLEMENTATION.md` - LLVM/MLIR/Flang implementation details
- `EVALUATION.md` - metrics, real Fortran test cases, baseline comparison, results
- `build.sh`, `run.sh`, and `package_release.sh` - Ubuntu entrypoints
- `scripts/build.sh` and `scripts/run.sh` - compatibility wrappers for the old script paths
- `src/` - source-level transformation helper
- `tools/` - `fiap-opt` driver
- `lib/` and `include/` - C++ analysis/pass implementation
- `testcases/fortran/` - real Fortran inputs for the main end-to-end pipeline
- `testcases/fortran_optimized/` - baseline comparison programs
- `profiles/` - optional seed profile data
- `upstream/` - patch-kit notes for wiring FIAP into a Flang checkout
- `docs/correctness.md` - rewrite preconditions and correctness argument
- `docs/release.md` - verification and release packaging checklist

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

## Build

On Ubuntu, `./build.sh` automatically installs the required apt packages when they are missing:

```bash
./build.sh
```

To skip automatic package installation after setting up dependencies yourself:

```bash
FIAP_INSTALL_DEPS=0 ./build.sh
```

For a custom local LLVM/Flang build:

```bash
LLVM_DIR=~/llvm-project/build/lib/cmake/llvm \
MLIR_DIR=~/llvm-project/build/lib/cmake/mlir \
Flang_DIR=~/llvm-project/build/lib/cmake/flang \
./build.sh
```

## Run Full End-To-End Demo

This is the main command to show:

```bash
./run.sh
```

The default run performs the real pipeline:

1. Reads the `.f90` files from `testcases/fortran/`.
2. Uses Flang to emit HLFIR/MLIR into `reports/hlfir/*.mlir`.
3. Runs `build/fiap-opt` over that generated HLFIR.
4. Writes FIAP JSON reports into `reports/hlfir/*.json`.
5. Writes `reports/hlfir/summary.csv` with strict expected-classification checks.
6. Applies safe HLFIR/FIR rewrites into `reports/hlfir/*.transformed.mlir`.
7. Rewrites safe source cases into `reports/source/*.transformed.f90`.
8. Validates transformed MLIR with `fir-opt` and transformed source with `flang -fsyntax-only`.
9. Generates profile-site data into `reports/profile/generated_profile.csv`.
10. Refines the real `function_result.f90` report using generated profile evidence.
11. Compiles and times original vs optimized Fortran programs into `reports/benchmark/runtime.csv`, including output-equivalence checks.

## Real Fortran Test Cases

| Source file | Construct | Expected primary classification |
| --- | --- | --- |
| `testcases/fortran/vector_add.f90` | rank-1 array expression | `provably-eliminable` |
| `testcases/fortran/matrix_stencil.f90` | 2D elemental temporary | `provably-eliminable` |
| `testcases/fortran/function_result.f90` | array-valued function result | `possibly-unnecessary` |
| `testcases/fortran/allocatable_update.f90` | allocatable assignment/reallocation | `possibly-unnecessary` |
| `testcases/fortran/escaping_temp.f90` | escaping temporary passed to call | `necessary` |
| `testcases/fortran/assumed_shape_kernel.f90` | assumed-shape descriptor expression | `necessary` |
| `testcases/fortran/saxpy_real_kernel.f90` | SAXPY-style whole-array update | `provably-eliminable` |
| `testcases/fortran/laplace2d_real_kernel.f90` | 2D Laplace-style stencil update | `provably-eliminable` |
| `testcases/fortran/option_pricing_real_kernel.f90` | pricing-style vector expression | `provably-eliminable` |
| `testcases/fortran/polybench_jacobi1d.f90` | PolyBench-style Jacobi update | `provably-eliminable` |
| `testcases/fortran/rank3_tensor_update.f90` | rank-3 tensor expression | `provably-eliminable` |
| `testcases/fortran/pointer_alias.f90` | pointer-backed alias-sensitive expression | `necessary` |
| `testcases/fortran/strided_section_update.f90` | strided array section expression | `necessary` |

The failure cases include `escaping_temp.f90`, `assumed_shape_kernel.f90`, `pointer_alias.f90`, and `strided_section_update.f90`: FIAP marks them `necessary` and does not apply a local transform.

## Run One Generated HLFIR Report

After `./run.sh`, inspect a generated HLFIR report directly:

```bash
build/fiap-opt reports/hlfir/vector_add.mlir --format=text
build/fiap-opt reports/hlfir/escaping_temp.mlir --format=text
build/fiap-opt reports/hlfir/vector_add.mlir --format=sarif
```

The second command is the failure case. The SARIF command emits IDE/code-scanning friendly diagnostics for the same allocation sites.

## HLFIR/FIR Rewrite Demo

After `./run.sh`, inspect the actual rewritten compiler IR:

```bash
build/fiap-opt reports/hlfir/vector_add.mlir --apply-transforms --print-annotated-ir
```

The generated evidence files are:

```text
reports/hlfir/vector_add.transformed.mlir
reports/hlfir/matrix_stencil.transformed.mlir
reports/hlfir/*_real_kernel.transformed.mlir
reports/hlfir/transforms.csv
```

The transform pass replaces safe `hlfir.elemental` array-expression assignments with explicit `fir.do_loop` nests, including the rank-3 tensor case. Stack promotion for `fir.allocmem` is implemented with strict guards and is skipped for true allocatable heap storage such as `fir.must_be_heap`. Reports and IR annotations include legality, shape evidence, alias evidence, and typed-Flang-match fields.

To verify generated evidence after a full run:

```bash
python3 scripts/check_pipeline_outputs.py --reports-dir reports --require-benchmark
```

## Real Fortran To HLFIR Command

You can also run only the Fortran-to-HLFIR analysis:

```bash
python3 scripts/analyze_fortran_hlfir.py \
  --flang /usr/lib/llvm-18/bin/flang-new \
  --tool build/fiap-opt \
  --source-dir testcases/fortran \
  --out-dir reports/hlfir \
  --summary reports/hlfir/summary.csv \
  --strict
```

## Source Transformation Demo

The transform consumes the generated real-HLFIR report, not a mock report:

```bash
python3 src/fiap_source_transformer.py \
  --report reports/hlfir/vector_add.json \
  --source testcases/fortran/vector_add.f90 \
  --output reports/source/vector_add.transformed.f90
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

The transformer also handles higher-rank expressions up to Fortran's rank-15 limit, real-kernel expressions, allocatable shape guards, and the simple function-result-to-result-buffer rewrite in `function_result.f90`.

## Profile Sites

FIAP can emit profile-site CSV rows directly from generated HLFIR:

```bash
build/fiap-opt reports/hlfir/function_result.mlir --emit-profile-sites
```

The full pipeline turns generated report entries into `reports/profile/generated_profile.csv`, then uses that file for profile-guided refinement.

## Current Status

Implemented:

- standalone C++ MLIR tool `fiap-opt`
- real `.f90 -> Flang HLFIR -> FIAP` workflow
- Allocation Provenance Graph construction
- conservative allocation classification
- source-location mapping
- byte estimation for static HLFIR/FIR array types
- assignment-compatible shape evidence for dynamic HLFIR/FIR values
- JSON/text/DOT reporting
- profile-site CSV reporting
- IR annotation with `fiap.*` metadata
- real HLFIR scalarization into `fir.do_loop` nests for safe elemental assignments, including nested elementals and rank-3+ arrays
- guarded FIR `fir.allocmem` to `fir.alloca` stack-promotion rewrite for bounded compiler temporaries
- source rewrites for rank-1 through rank-15 array expressions, allocatable shape guards, and simple function-result result-buffer conversion
- generated profile-site data and profile-guided refinement
- thirteen real Fortran test cases including four larger real-kernel-style workloads, descriptor/section negative cases, a rank-3 positive case, and a pointer-alias negative case
- baseline-vs-optimized Fortran benchmark harness with output-equivalence checks
- SARIF reports for every generated HLFIR input
- CTest evidence gate, lit-style generated-evidence regression checks, and GitHub Actions Ubuntu full-pipeline workflow
- formal local correctness argument for the implemented rewrites
- release packaging scripts for GitHub distribution

Beyond-submission extensions:

- add more external benchmark suites such as LAPACK or SPEC CPU when those sources are available locally
- land the upstream integration patch inside a private or upstream LLVM checkout
