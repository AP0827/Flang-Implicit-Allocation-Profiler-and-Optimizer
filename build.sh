#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-"$ROOT_DIR/build"}"
LLVM_DIR="${LLVM_DIR:-}"
MLIR_DIR="${MLIR_DIR:-}"
FIAP_INSTALL_DEPS="${FIAP_INSTALL_DEPS:-auto}"
FIAP_ENABLE_FLANG_CPP="${FIAP_ENABLE_FLANG_CPP:-1}"
REQUESTED_FLANG_DIR="${Flang_DIR:-${FLANG_DIR:-}}"
FLANG_DIR=""
LOG_DIR="$BUILD_DIR/logs"
LOG_FILE="$LOG_DIR/build.log"
APT_PACKAGES=(
  build-essential
  cmake
  ninja-build
  python3
  zip
  llvm-18
  llvm-18-dev
  llvm-18-tools
  llvm-18-linker-tools
  libclang-cpp18-dev
  libmlir-18-dev
  mlir-18-tools
  flang-18
  libflang-18-dev
  libffi-dev
  libncurses-dev
  libz3-dev
  libxml2-dev
  libzstd-dev
  zlib1g-dev
)

find_package_dir() {
  local package="$1"
  shift
  local candidate

  for candidate in "$@"; do
    if [[ -d "$candidate" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  return 1
}

derive_sibling_package_dir() {
  local current_dir="$1"
  local sibling="$2"

  if [[ "$current_dir" == */lib/cmake/* ]]; then
    local candidate="${current_dir%/*}/$sibling"
    if [[ -d "$candidate" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  fi

  return 1
}

derive_llvm_prefix() {
  local llvm_dir="$1"

  if [[ "$llvm_dir" == */lib/cmake/llvm ]]; then
    printf '%s\n' "${llvm_dir%/lib/cmake/llvm}"
    return 0
  fi

  return 1
}

run_apt_get() {
  if [[ "$EUID" -eq 0 ]]; then
    apt-get "$@"
    return
  fi

  if ! command -v sudo >/dev/null 2>&1; then
    echo "error: sudo is required to install Ubuntu packages automatically" >&2
    echo "install the packages from README.md, or rerun with FIAP_INSTALL_DEPS=0 after installing them" >&2
    exit 127
  fi

  sudo apt-get "$@"
}

ubuntu_dependencies_ready() {
  local missing=0

  command -v cmake >/dev/null 2>&1 || missing=1
  command -v python3 >/dev/null 2>&1 || missing=1
  command -v c++ >/dev/null 2>&1 || missing=1

  [[ -d /usr/lib/llvm-18/lib/cmake/llvm ]] || missing=1
  [[ -d /usr/lib/llvm-18/lib/cmake/mlir ]] || missing=1
  [[ -d /usr/lib/llvm-18/lib/cmake/flang ]] || missing=1
  [[ -x /usr/lib/llvm-18/bin/flang-new || -x /usr/lib/llvm-18/bin/flang ]] || missing=1
  [[ -e /usr/lib/llvm-18/lib/libclang-cpp.so || -e /usr/lib/x86_64-linux-gnu/libclang-cpp.so ]] || missing=1

  return "$missing"
}

install_ubuntu_dependencies_if_needed() {
  if [[ "$FIAP_INSTALL_DEPS" == "0" || "$FIAP_INSTALL_DEPS" == "false" ]]; then
    return
  fi

  if ! command -v apt-get >/dev/null 2>&1; then
    return
  fi

  if [[ -n "$LLVM_DIR" && -n "$MLIR_DIR" ]]; then
    return
  fi

  if ubuntu_dependencies_ready; then
    return
  fi

  echo "Installing Ubuntu build dependencies"
  if [[ "${FIAP_SKIP_APT_UPDATE:-0}" != "1" ]]; then
    run_apt_get update
  fi
  run_apt_get install -y "${APT_PACKAGES[@]}"
}

install_ubuntu_dependencies_if_needed

if ! command -v cmake >/dev/null 2>&1; then
  echo "error: cmake is not installed or not on PATH" >&2
  echo "install CMake >= 3.24, then rerun this script" >&2
  exit 127
fi

if [[ -z "$LLVM_DIR" ]]; then
  if ! LLVM_DIR="$(find_package_dir llvm \
    /usr/lib/llvm-20/lib/cmake/llvm \
    /usr/lib/llvm-19/lib/cmake/llvm \
    /usr/lib/llvm-18/lib/cmake/llvm \
    /usr/lib/llvm-*/lib/cmake/llvm)"; then
    echo "error: LLVM_DIR is not set and no default LLVM CMake package was found" >&2
    echo "install llvm-18-dev, or set LLVM_DIR to the directory containing LLVMConfig.cmake" >&2
    exit 2
  fi
fi

if [[ -z "$MLIR_DIR" ]]; then
  if ! MLIR_DIR="$(derive_sibling_package_dir "$LLVM_DIR" mlir)"; then
    if ! MLIR_DIR="$(find_package_dir mlir \
      /usr/lib/llvm-20/lib/cmake/mlir \
      /usr/lib/llvm-19/lib/cmake/mlir \
      /usr/lib/llvm-18/lib/cmake/mlir \
      /usr/lib/llvm-*/lib/cmake/mlir)"; then
      echo "error: MLIR_DIR is not set and no default MLIR CMake package was found" >&2
      echo "install libmlir-18-dev, or set MLIR_DIR to the directory containing MLIRConfig.cmake" >&2
      exit 2
    fi
  fi
fi

if [[ "$FIAP_ENABLE_FLANG_CPP" == "1" ]]; then
  FLANG_DIR="$REQUESTED_FLANG_DIR"
  if [[ -z "$FLANG_DIR" ]] && ! FLANG_DIR="$(derive_sibling_package_dir "$LLVM_DIR" flang)"; then
    FLANG_DIR="$(find_package_dir flang \
      /usr/lib/llvm-20/lib/cmake/flang \
      /usr/lib/llvm-19/lib/cmake/flang \
      /usr/lib/llvm-18/lib/cmake/flang \
      /usr/lib/llvm-*/lib/cmake/flang)" || FLANG_DIR=""
  fi
fi

LLVM_PREFIX="$(derive_llvm_prefix "$LLVM_DIR" || true)"
if [[ -n "$LLVM_PREFIX" && -d "$LLVM_PREFIX/bin" ]]; then
  export PATH="$LLVM_PREFIX/bin:$PATH"
fi

CMAKE_PREFIX_ENTRIES=()
if [[ -n "$LLVM_PREFIX" ]]; then
  CMAKE_PREFIX_ENTRIES+=("$LLVM_PREFIX")
  if [[ "$LLVM_PREFIX" == */lib/llvm-* ]]; then
    CMAKE_PREFIX_ENTRIES+=("${LLVM_PREFIX%/lib/llvm-*}")
  fi
fi
if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
  CMAKE_PREFIX_ENTRIES+=("$CMAKE_PREFIX_PATH")
fi
CMAKE_PREFIX_PATH="$(IFS=';'; printf '%s' "${CMAKE_PREFIX_ENTRIES[*]}")"
export CMAKE_PREFIX_PATH

CMAKE_ARGS=(
  -S "$ROOT_DIR"
  -B "$BUILD_DIR"
  -DCMAKE_BUILD_TYPE=Release
  -DLLVM_DIR="$LLVM_DIR"
  -DMLIR_DIR="$MLIR_DIR"
)

if [[ -n "$FLANG_DIR" ]]; then
  CMAKE_ARGS+=(-DFlang_DIR="$FLANG_DIR")
else
  CMAKE_ARGS+=(-DCMAKE_DISABLE_FIND_PACKAGE_Flang=TRUE)
fi

mkdir -p "$LOG_DIR"
printf 'FIAP build log\n' > "$LOG_FILE"

echo
echo "FIAP build"
echo "log: $LOG_FILE"
echo "LLVM_DIR: $LLVM_DIR"
echo "MLIR_DIR: $MLIR_DIR"
if [[ -n "$FLANG_DIR" ]]; then
  echo "Flang_DIR: $FLANG_DIR"
else
  echo "Flang C++ integration: disabled (generic MLIR mode)"
fi

echo "[1/2] Configuring CMake"
if cmake "${CMAKE_ARGS[@]}" >>"$LOG_FILE" 2>&1; then
  echo "ok"
else
  echo "failed. See $LOG_FILE" >&2
  exit 1
fi

echo "[2/2] Building fiap-opt"
if cmake --build "$BUILD_DIR" --config Release >>"$LOG_FILE" 2>&1; then
  echo "ok"
else
  echo "failed. See $LOG_FILE" >&2
  exit 1
fi

echo
echo "Build complete"
