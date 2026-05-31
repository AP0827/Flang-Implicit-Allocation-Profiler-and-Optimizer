# Release Checklist

Use this checklist before sending the GitHub repository link or creating a
versioned release archive.

## Required Verification

```bash
./build.sh
./run.sh --benchmark-runs 5
python3 scripts/check_pipeline_outputs.py --reports-dir reports --require-benchmark
ctest --test-dir build --output-on-failure
```

## Optional Install Smoke

```bash
cmake --install build --prefix out/fiap-install
```

## Package

```bash
./package_release.sh 1.0.0
```

The package contains the FIAP source, scripts, testcases, documentation,
upstream integration notes, benchmarks, and CI metadata. It intentionally does
not vendor `llvm-project`.
