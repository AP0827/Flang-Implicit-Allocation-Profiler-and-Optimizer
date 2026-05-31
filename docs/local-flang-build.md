# Local Flang Build

## Goal

This project reaches its strongest form when built against a local `llvm-project` checkout that includes:

- LLVM
- MLIR
- Flang

The typed integration path in `fiap` is designed for that setup.

## 1. Build LLVM + MLIR + Flang

From an `llvm-project` checkout, configure a build like this:

```bash
cmake -S llvm -B build \
  -G Ninja \
  -DLLVM_ENABLE_PROJECTS="mlir;clang;flang" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_TARGETS_TO_BUILD="X86"
```

Then build:

```bash
ninja -C build
```

Useful outputs to confirm:

- `build/bin/flang-new`
- `build/bin/bbc` if your revision provides it
- `build/lib/cmake/llvm`
- `build/lib/cmake/mlir`
- `build/lib/cmake/flang`

## 2. Configure `fiap` Against That Build

```bash
cmake -S /home/asish/Flang-Implicit-Allocation-Profiler-and-Optimizer \
  -B /home/asish/Flang-Implicit-Allocation-Profiler-and-Optimizer/build \
  -DLLVM_DIR=~/llvm-project/build/lib/cmake/llvm \
  -DMLIR_DIR=~/llvm-project/build/lib/cmake/mlir \
  -DFlang_DIR=~/llvm-project/build/lib/cmake/flang
```

Then build:

```bash
cmake --build /home/asish/Flang-Implicit-Allocation-Profiler-and-Optimizer/build
```

## 3. Emit HLFIR

Use your local Flang binary to lower a Fortran file into HLFIR or FIR:

```bash
flang-new -fc1 -emit-hlfir your_program.f90 -o your_program.mlir
```

If your build exposes `bbc`, a common alternative is:

```bash
bbc -emit-hlfir your_program.f90 -o your_program.mlir
```

## 4. Run `fiap`

```bash
/home/asish/Flang-Implicit-Allocation-Profiler-and-Optimizer/build/fiap-opt your_program.mlir
```

For JSON:

```bash
/home/asish/Flang-Implicit-Allocation-Profiler-and-Optimizer/build/fiap-opt your_program.mlir --format=json
```

For graph output:

```bash
/home/asish/Flang-Implicit-Allocation-Profiler-and-Optimizer/build/fiap-opt your_program.mlir --format=dot
```

## 5. Move From Prototype To Compiler Pass

Once the project is building against your exact Flang revision, the best next edits are:

- replace remaining generic operation-name checks with typed `fir::` and `hlfir::` matches
- implement one end-to-end typed rewrite for a simple `hlfir.assign`
- validate the resulting IR with Flang-generated test inputs
- add benchmark runs and collect JSON reports

## Recommended First Compiler Rewrite

Start with:

- rank-1 array expressions
- single consumer
- no alias edges
- static or shape-constrained extents

That gives you the cleanest path to replacing an implicit temporary with direct loop-based stores into the destination.
