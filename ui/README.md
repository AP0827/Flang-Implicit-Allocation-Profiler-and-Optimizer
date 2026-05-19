# FIAP Demo Console

This Vite dashboard is a local UI for the Flang Implicit Allocation Profiler project. It runs a small Node server that exposes only fixed project tasks, then renders the generated reports in the browser.

## Run

From the repository root:

```powershell
scripts\run-dashboard.ps1
```

Or directly:

```powershell
cd ui
npm install
npm run dev
```

Open:

```text
http://127.0.0.1:5173
```

## What The Buttons Do

- build `fiap-opt`
- run the five MLIR analysis testcases
- regenerate `reports/evaluation-summary.csv`
- create allocation JSON reports
- run profile-guided refinement
- generate the source-level rewrite demo
- run the baseline-vs-optimized Fortran benchmark
- emit and analyze real HLFIR from Flang

The backend intentionally uses an allowlist of tasks rather than accepting arbitrary shell commands.
