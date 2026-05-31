#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-"$ROOT_DIR/build"}"
FORMAT="${FORMAT:-text}"

find_executable() {
  local env_value="$1"
  shift
  local candidate

  if [[ -n "$env_value" && -x "$env_value" ]]; then
    printf '%s\n' "$env_value"
    return 0
  fi

  for candidate in "$@"; do
    if command -v "$candidate" >/dev/null 2>&1; then
      command -v "$candidate"
      return 0
    fi
    if [[ -x "$candidate" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  return 1
}

has_option() {
  local option="$1"
  shift
  local arg

  for arg in "$@"; do
    if [[ "$arg" == "$option" || "$arg" == "$option="* ]]; then
      return 0
    fi
  done

  return 1
}

for llvm_bin in \
  /usr/lib/llvm-20/bin \
  /usr/lib/llvm-19/bin \
  /usr/lib/llvm-18/bin \
  /usr/lib/llvm-*/bin; do
  if [[ -d "$llvm_bin" ]]; then
    export PATH="$llvm_bin:$PATH"
    break
  fi
done

TOOL="$BUILD_DIR/fiap-opt"

if [[ ! -x "$TOOL" ]]; then
  echo "error: fiap-opt was not found in $BUILD_DIR" >&2
  echo "run ./build.sh first" >&2
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

PIPELINE_ARGS=(--tool "$TOOL" --build-dir "$BUILD_DIR")

if ! has_option --flang "$@"; then
  if FIAP_FLANG_RESOLVED="$(find_executable "${FIAP_FLANG:-${FLANG:-}}" \
    flang-new flang flang-new-20 flang-new-19 flang-new-18 flang-20 flang-19 flang-18 \
    /usr/lib/llvm-20/bin/flang-new \
    /usr/lib/llvm-19/bin/flang-new \
    /usr/lib/llvm-18/bin/flang-new \
    /usr/lib/llvm-20/bin/flang \
    /usr/lib/llvm-19/bin/flang \
    /usr/lib/llvm-18/bin/flang)"; then
    PIPELINE_ARGS+=(--flang "$FIAP_FLANG_RESOLVED")
  fi
fi

if ! has_option --fir-opt "$@"; then
  if FIAP_FIR_OPT_RESOLVED="$(find_executable "${FIAP_FIR_OPT:-${FIR_OPT:-}}" \
    fir-opt fir-opt-20 fir-opt-19 fir-opt-18 \
    /usr/lib/llvm-20/bin/fir-opt \
    /usr/lib/llvm-19/bin/fir-opt \
    /usr/lib/llvm-18/bin/fir-opt)"; then
    PIPELINE_ARGS+=(--fir-opt "$FIAP_FIR_OPT_RESOLVED")
  fi
fi

"$PYTHON" "$ROOT_DIR/scripts/run_pipeline.py" "${PIPELINE_ARGS[@]}" "$@"
