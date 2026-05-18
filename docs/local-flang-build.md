# Local Flang Build

## Goal

This project reaches its strongest form when built against a real local `llvm-project` checkout that includes:

- LLVM
- MLIR
- Flang

The typed integration path in `fiap` is designed for that setup.

## 1. Build LLVM + MLIR + Flang

From an `llvm-project` checkout, configure a build like this:

```powershell
cmake -S llvm -B build `
  -G Ninja `
  -DLLVM_ENABLE_PROJECTS="mlir;clang;flang" `
  -DCMAKE_BUILD_TYPE=Release `
  -DLLVM_TARGETS_TO_BUILD="X86"
```

Then build:

```powershell
ninja -C build
```

Useful outputs to confirm:

- `build\bin\flang-new.exe`
- `build\bin\bbc.exe` if your revision provides it
- `build\lib\cmake\llvm`
- `build\lib\cmake\mlir`
- `build\lib\cmake\flang`

## 2. Configure `fiap` Against That Build

```powershell
cmake -S D:\FlangImplicitAllocationProfiler -B D:\FlangImplicitAllocationProfiler\build `
  -DLLVM_DIR=D:\llvm-project\build\lib\cmake\llvm `
  -DMLIR_DIR=D:\llvm-project\build\lib\cmake\mlir `
  -DFlang_DIR=D:\llvm-project\build\lib\cmake\flang
```

Then build:

```powershell
cmake --build D:\FlangImplicitAllocationProfiler\build --config Release
```

## 3. Emit Real HLFIR

Use your local Flang binary to lower a Fortran file into HLFIR or FIR:

```powershell
flang-new -fc1 -emit-hlfir your_program.f90 -o your_program.mlir
```

If your build exposes `bbc`, a common alternative is:

```powershell
bbc -emit-hlfir your_program.f90 -o your_program.mlir
```

## 4. Run `fiap`

```powershell
D:\FlangImplicitAllocationProfiler\build\fiap-opt.exe your_program.mlir
```

For JSON:

```powershell
D:\FlangImplicitAllocationProfiler\build\fiap-opt.exe your_program.mlir --format=json
```

For graph output:

```powershell
D:\FlangImplicitAllocationProfiler\build\fiap-opt.exe your_program.mlir --format=dot
```

## 5. Move From Prototype To Real Compiler Pass

Once the project is building against your exact Flang revision, the best next edits are:

- replace remaining generic operation-name checks with typed `fir::` and `hlfir::` matches
- implement one end-to-end typed rewrite for a simple `hlfir.assign`
- validate the resulting IR with real Flang test inputs
- add benchmark runs and collect JSON reports

## Recommended First Real Rewrite

Start with:

- rank-1 array expressions
- single consumer
- no alias edges
- static or shape-constrained extents

That gives you the cleanest path to replacing an implicit temporary with direct loop-based stores into the destination.
