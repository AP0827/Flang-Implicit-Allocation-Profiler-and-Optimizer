# Benchmarks

Use this directory to store the three evaluation workloads required by the assignment.

Suggested categories:

- a synthetic array-expression microbenchmark
- an allocatable-resize benchmark
- a real numerical code with repeated array temporaries

For each benchmark, collect:

- baseline runtime
- runtime with analysis only
- runtime after transformations
- number of hidden allocations
- bytes allocated in temporaries

The file `results-template.csv` gives you a simple starting schema for collecting these measurements across workloads.
