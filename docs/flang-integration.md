# Flang Integration Notes

FIAP is implemented as reusable MLIR passes plus a standalone driver. The same
passes can be wired into a Flang/MLIR pipeline through:

- `fiap::registerFIAPPasses()`
- `fiap::createImplicitAllocationProfilerPass()`
- `fiap::createPromoteTempToStackPass()`
- `fiap::createScalarizeArrayExprPass()`

The registered pipeline name is:

```text
fiap-profile-and-transform
```

An upstream Flang integration would call `fiap::registerFIAPPasses()` during
pass registration and insert the pipeline after HLFIR generation, before
general lowering destroys high-level array-expression structure.

Recommended position:

```text
Fortran source
  -> semantic checks
  -> HLFIR generation
  -> FIAP analysis/report pass
  -> guarded FIAP HLFIR/FIR rewrites
  -> normal Flang lowering/optimization pipeline
```

The standalone `fiap-opt` tool remains the course-project driver because it is
easy to build, demo, and evaluate without patching an external LLVM checkout.
