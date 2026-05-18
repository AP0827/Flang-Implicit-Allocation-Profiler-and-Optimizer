# Architecture

## Problem

Fortran array syntax looks cheap in source, but Flang may synthesize heap allocation for:

- array expression temporaries
- array-valued function results
- automatic realloc-on-assignment for allocatables
- shape adaptation introduced by HLFIR/FIR lowering

The user never sees these allocations in source. This project inspects HLFIR/FIR where the compiler-created operations are explicit enough to analyze.

## Allocation Provenance Graph

The central abstraction is the `Allocation Provenance Graph` (APG).

### Node kinds

- `Expression`: HLFIR expression-producing operations such as `hlfir.as_expr`
- `Associate`: `hlfir.associate` regions that materialize temporary lifetimes
- `AllocMem`: explicit heap allocation sites like `fir.allocmem`
- `Assign`: `hlfir.assign` or FIR assignment-like operations that consume temporaries

### Edge kinds

- `Produces`: one operation creates a value whose realization requires another node
- `Consumes`: an assignment or call consumes a temporary
- `ShapeConstrains`: shape information used in classification
- `Aliases`: potential alias relation that blocks scalarization or stack promotion

### Node annotations

- source location
- estimated extent in elements and bytes
- loop depth
- escape behavior
- target classification

## Pass Pipeline

### 1. Discovery

Walk HLFIR/FIR operations and collect candidate implicit allocation sites. The current prototype recognizes operations by name so it can run either against a full Flang build or against generic MLIR assembly with unregistered FIR/HLFIR operations.

### 2. Graph construction

Convert candidates into APG nodes and connect them using producer-consumer and shape relationships.

### 3. Classification

Run a monotone classifier over the lattice:

`provably-eliminable < possibly-unnecessary < necessary`

### 4. Reporting

Emit source-facing diagnostics such as:

`line 42: A = B + C generates an implicit temporary array allocation (~10 MB)`

The stronger prototype also emits:

- JSON for benchmark pipelines
- DOT for APG visualization
- IR annotations for downstream passes

### 5. Transformation

Apply one of two conservative rewrites:

- stack promotion of bounded temporaries
- scalarization of simple element-wise array expressions

In the current codebase these rewrites are represented as transformation-preparation passes. They attach lowering hints and explicit rewrite templates so the project remains robust across Flang versions while still demonstrating the optimization path.

## Current Prototype Coverage

The current implementation focuses on:

- single-statement array expression temporaries
- allocatable assignment reallocation detection
- function-result temporaries with one direct consumer
- generic MLIR examples that exercise the pipeline without needing a complete Flang build

The project deliberately avoids whole-program alias reasoning in the current milestone.
