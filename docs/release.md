# Release Checklist

Use this checklist before sending the GitHub repository link or creating a
versioned release archive.

## Required Verification

```powershell
.\scripts\build.ps1
.\scripts\run.ps1 -BenchmarkRuns 5
python .\scripts\check_pipeline_outputs.py --reports-dir .\reports --require-benchmark
ctest --test-dir .\build --output-on-failure
```

## Optional Install Smoke

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --install .\build --prefix .\out\fiap-install
```

## Package

```powershell
.\scripts\package_release.ps1 -Version 1.0.0
```

The package contains the FIAP source, scripts, testcases, documentation,
upstream integration notes, benchmarks, and CI metadata. It intentionally does
not vendor `llvm-project`.
