# EVALUATION

## Metrics

The evaluation reports:

- number of allocation-bearing sites
- count of `provably-eliminable`, `possibly-unnecessary`, and `necessary` sites
- estimated bytes attributed to implicit allocations
- candidate allocation reduction after applying provably eliminable transformations
- baseline-vs-optimized expectation for the three representative Fortran kernels

The automated evaluator is `scripts/evaluate.py`. It runs `fiap-opt --format=json` over every `.mlir` testcase and writes `reports/evaluation-summary.csv`.

`scripts/benchmark_fortran.py` measures wall-clock speedup for three baseline/optimized Fortran program pairs when a Fortran compiler is available.

## Baseline Comparison Method

Baseline is the original lowered IR: all detected allocation-bearing sites remain.

Optimized estimate removes only sites classified as `provably-eliminable`. The reduction is therefore conservative:

```text
optimized estimated bytes = total estimated bytes - provably eliminable estimated bytes
```

For speedup measurement on a full machine, compile the original and transformed Fortran kernels with the same Flang optimization flags, run each kernel repeatedly, and compare:

- wall-clock time
- heap allocation count
- allocated bytes
- peak RSS

The repository includes three representative Fortran programs in `testcases/fortran/` and optimized counterparts in `testcases/fortran_optimized/`:

- vector expression temporary
- matrix stencil/expression temporary
- allocatable assignment/function-result pattern

Runtime command:

```bash
python scripts/benchmark_fortran.py --out reports/runtime-benchmark.csv
```

If no Fortran compiler is installed, the script reports the missing dependency. It does not fabricate speedup numbers.

## Profile-Guided Refinement Evaluation

The sample profile demonstrates how ambiguous sites become eliminable under stable runtime shapes:

```bash
build/fiap-opt.exe testcases/02_function_result.mlir --format=json > reports/function_result.json
python scripts/refine_profile.py --report reports/function_result.json --profile profiles/sample_profile.csv --out reports/function_result.refined.json
```

This corresponds to the PGAE loop in the research plan.

## Testcase Matrix

| Testcase | Construct | Expected classification |
| --- | --- | --- |
| `01_array_temp.mlir` | `A = B + C` temporary | `provably-eliminable` |
| `02_function_result.mlir` | array-valued function result | `possibly-unnecessary` |
| `03_realloc_assignment.mlir` | realloc-on-assignment | `possibly-unnecessary` |
| `04_escaping_temp.mlir` | temporary passed to call | `necessary` |
| `05_elemental_temp.mlir` | elemental temporary | `provably-eliminable` |

## Demo Evidence To Capture

For the final submission, include screenshots or a short video showing:

1. `./scripts/build.sh` completing.
2. `./scripts/run.sh testcases/01_array_temp.mlir`.
3. `./scripts/run.sh testcases/04_escaping_temp.mlir` showing the failure/necessary case.
4. `python src/fiap_source_transformer.py --report reports/01_array_temp.json --source testcases/fortran/vector_add.f90 --output reports/vector_add.transformed.f90`.
5. `python scripts/evaluate.py --tool build/fiap-opt --testcases testcases --out reports/evaluation-summary.csv`.
6. `python scripts/refine_profile.py --report reports/function_result.json --profile profiles/sample_profile.csv --out reports/function_result.refined.json`.
7. `python scripts/benchmark_fortran.py --out reports/runtime-benchmark.csv`, or the missing-compiler message if no Fortran compiler is installed.

## Current Limitation

This prototype estimates allocation impact from IR evidence and measures runtime speedup when a Fortran compiler is available. On systems without `flang-new`, `flang`, `gfortran`, or `ifx`, runtime benchmarking is blocked by installation, not by missing project code.
