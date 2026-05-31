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

Walk HLFIR/FIR operations and collect candidate implicit allocation sites. The workflow obtains this IR by running Flang on real `.f90` files.

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

In the current codebase these rewrites are real typed transform passes when Flang support is available. The scalarization pass replaces safe `hlfir.elemental` assignments with explicit `fir.do_loop` nests, and the stack-promotion pass rewrites only guarded bounded temporaries.

## Current Coverage

The current implementation covers the required assignment classes and the submission evidence uses real Flang-generated HLFIR:

- single-statement and nested elemental array-expression temporaries
- allocatable assignment reallocation detection
- function-result temporaries with one direct consumer
- pointer-alias, assumed-shape descriptor, strided-section, and escaping-temporary negative cases
- rank-3 and larger real-kernel-style positive cases
- thirteen real Fortran inputs lowered through Flang before FIAP analysis

FIAP uses conservative local alias and shape evidence. Risky descriptor, pointer-like, target-like, class-like, escaping, and strided-section cases are reported but not rewritten unless the classifier proves a local rewrite is legal.
