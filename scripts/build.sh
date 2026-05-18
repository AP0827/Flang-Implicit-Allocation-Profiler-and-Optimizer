#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"$ROOT_DIR/build"}"
LLVM_DIR="${LLVM_DIR:-}"
MLIR_DIR="${MLIR_DIR:-}"
FLANG_DIR="${Flang_DIR:-${FLANG_DIR:-}}"

if ! command -v cmake >/dev/null 2>&1; then
  echo "error: cmake is not installed or not on PATH" >&2
  echo "install CMake >= 3.24, then rerun this script" >&2
  exit 127
fi

if [[ -z "$LLVM_DIR" ]]; then
  if [[ -d "/d/llvm-project/build/lib/cmake/llvm" ]]; then
    LLVM_DIR="/d/llvm-project/build/lib/cmake/llvm"
  elif [[ -d "/mnt/d/llvm-project/build/lib/cmake/llvm" ]]; then
    LLVM_DIR="/mnt/d/llvm-project/build/lib/cmake/llvm"
  elif [[ -d "D:/llvm-project/build/lib/cmake/llvm" ]]; then
    LLVM_DIR="D:/llvm-project/build/lib/cmake/llvm"
  else
    echo "error: LLVM_DIR is not set and no default LLVM CMake package was found" >&2
    exit 2
  fi
fi

if [[ -z "$MLIR_DIR" ]]; then
  if [[ -d "/d/llvm-project/build/lib/cmake/mlir" ]]; then
    MLIR_DIR="/d/llvm-project/build/lib/cmake/mlir"
  elif [[ -d "/mnt/d/llvm-project/build/lib/cmake/mlir" ]]; then
    MLIR_DIR="/mnt/d/llvm-project/build/lib/cmake/mlir"
  elif [[ -d "D:/llvm-project/build/lib/cmake/mlir" ]]; then
    MLIR_DIR="D:/llvm-project/build/lib/cmake/mlir"
  else
    echo "error: MLIR_DIR is not set and no default MLIR CMake package was found" >&2
    exit 2
  fi
fi

CMAKE_ARGS=(
  -S "$ROOT_DIR"
  -B "$BUILD_DIR"
  -DCMAKE_BUILD_TYPE=Release
  -DLLVM_DIR="$LLVM_DIR"
  -DMLIR_DIR="$MLIR_DIR"
)

if [[ -n "$FLANG_DIR" ]]; then
  CMAKE_ARGS+=(-DFlang_DIR="$FLANG_DIR")
fi

cmake "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" --config Release
