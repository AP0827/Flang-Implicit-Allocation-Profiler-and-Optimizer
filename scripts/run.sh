#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"$ROOT_DIR/build"}"
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

if [[ $# -gt 0 && "$1" == *.mlir ]]; then
  INPUT="$1"
  shift
  "$TOOL" "$INPUT" --format="$FORMAT" "$@"
  exit 0
fi

PYTHON="${PYTHON:-python3}"
if ! command -v "$PYTHON" >/dev/null 2>&1; then
  PYTHON="python"
fi

"$PYTHON" "$ROOT_DIR/scripts/run_backend_demo.py" --tool "$TOOL" --build-dir "$BUILD_DIR" "$@"
