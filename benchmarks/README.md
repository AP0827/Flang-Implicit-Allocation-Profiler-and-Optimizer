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
python scripts/benchmark_fortran.py --out reports/benchmark/runtime.csv
```

Windows with the local Flang build:

```powershell
python scripts\benchmark_fortran.py `
  --compiler D:\llvm-project\build\bin\flang.exe `
  --out reports\benchmark\runtime.csv `
  --runs 20
```

The benchmark script:

- discovers a compiler if one is not provided
- uses the Visual Studio developer environment automatically on Windows
- compiles baseline and optimized programs with the same compiler flags
- writes `baseline_seconds`, `optimized_seconds`, `speedup_percent`, and `status`

Small kernels can be noisy, so runtime speedup is supporting evidence. The primary deterministic metric is the static allocation reduction in `reports/hlfir/summary.csv`.
