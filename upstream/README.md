# Upstream Integration Kit

This directory is a reviewer-facing bridge between the standalone FIAP project
and an eventual `llvm-project/flang` integration.

The project already exposes reusable C++ entry points:

- `fiap::registerFIAPPasses()`
- `fiap::createImplicitAllocationProfilerPass()`
- `fiap::createPromoteTempToStackPass()`
- `fiap::createScalarizeArrayExprPass()`

The pass pipeline registered by FIAP is:

```text
fiap-profile-and-transform
```

`flang-fiap-integration.patch` is not applied automatically by the submission
scripts because it targets an external LLVM checkout. It documents the minimal
kind of upstream wiring needed: link the FIAP library beside Flang optimizer
components, register FIAP passes during Flang pass registration, and insert the
pipeline after HLFIR generation while array-expression structure is still
available.
