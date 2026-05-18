#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"$ROOT_DIR/build"}"
INPUT="${1:-"$ROOT_DIR/testcases/01_array_temp.mlir"}"
FORMAT="${FORMAT:-text}"

TOOL="$BUILD_DIR/fiap-opt"
if [[ ! -x "$TOOL" && -x "$BUILD_DIR/Release/fiap-opt.exe" ]]; then
  TOOL="$BUILD_DIR/Release/fiap-opt.exe"
elif [[ ! -x "$TOOL" && -x "$BUILD_DIR/fiap-opt.exe" ]]; then
  TOOL="$BUILD_DIR/fiap-opt.exe"
fi

if [[ ! -x "$TOOL" ]]; then
  echo "error: fiap-opt was not found in $BUILD_DIR" >&2
  echo "run ./scripts/build.sh first" >&2
  exit 2
fi

"$TOOL" "$INPUT" --format="$FORMAT" "${@:2}"
