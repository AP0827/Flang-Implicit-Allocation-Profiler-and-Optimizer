# Flang Implicit Allocation Profiler and Optimizer

This repository contains a lightweight, source-mapped analysis scaffold for finding implicit heap allocations in Fortran code paths that Flang lowers through HLFIR/FIR.

It focuses on three classes of allocation pressure:

- array-expression temporaries
- array-valued function results
- automatic reallocation on assignment

The current implementation is a self-contained Python prototype that can:

- ingest a structured IR dump or analysis record set
- classify each allocation site as provably unnecessary, possibly unnecessary, or necessary
- generate a source-facing report
- suggest or apply a simple loop-based rewrite for straightforward temporary-array cases

## Project Layout

- `src/flang_alloc_profiler/` contains the analyzer, report generator, and transformation helpers
- `tests/` contains unit tests for the classification logic

## Usage

Install the package in editable mode first:

```bash
python -m pip install -e .
```

Analyze a structured allocation dump:

```bash
python -m flang_alloc_profiler.cli analyze --input examples/sample_analysis.json
```

Generate a source rewrite for a simple temporary:

```bash
python -m flang_alloc_profiler.cli transform --source examples/simple.f90 --input examples/sample_analysis.json
```

## Notes

The Flang integration point is intentionally modeled as a JSON-friendly record format so the core logic can be developed and tested without a full LLVM build in the workspace. The next step is wiring a real HLFIR/FIR extraction pass into that record format.

