# Benchmarks

The benchmark path compares original Fortran kernels against manually optimized equivalents.

Inputs:

- `testcases/fortran/vector_add.f90`
- `testcases/fortran/matrix_stencil.f90`
- `testcases/fortran/function_result.f90`
- `testcases/fortran/allocatable_update.f90`
- `testcases/fortran/escaping_temp.f90`

Optimized counterparts:

- `testcases/fortran_optimized/vector_add.f90`
- `testcases/fortran_optimized/matrix_stencil.f90`
- `testcases/fortran_optimized/function_result.f90`
- `testcases/fortran_optimized/allocatable_update.f90`
- `testcases/fortran_optimized/escaping_temp.f90`

Run:

```bash
python3 scripts/benchmark_fortran.py --out reports/benchmark/runtime.csv
```

Ubuntu with an explicit Flang install:

```bash
python3 scripts/benchmark_fortran.py \
  --compiler /usr/lib/llvm-18/bin/flang-new \
  --out reports/benchmark/runtime.csv \
  --runs 20
```

The benchmark script:

- discovers a compiler if one is not provided
- compiles baseline and optimized programs with the same compiler flags
- writes `baseline_seconds`, `optimized_seconds`, `speedup_percent`, and `status`

Small kernels can be noisy, so runtime speedup is supporting evidence. The primary deterministic metric is the static allocation reduction in `reports/hlfir/summary.csv`.
