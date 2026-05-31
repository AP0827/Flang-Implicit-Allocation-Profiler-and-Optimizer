#!/usr/bin/env bash
set -euo pipefail

VERSION="${1:-1.0.0}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/out"
STAGE="$OUT_DIR/fiap-$VERSION"
ZIP="$OUT_DIR/fiap-$VERSION.zip"

rm -rf "$STAGE" "$ZIP"
mkdir -p "$STAGE"

for item in \
  README.md DESIGN.md IMPLEMENTATION.md EVALUATION.md CMakeLists.txt \
  cmake include lib tools src scripts testcases docs profiles benchmarks upstream .github; do
  if [[ -e "$ROOT_DIR/$item" ]]; then
    cp -R "$ROOT_DIR/$item" "$STAGE/"
  fi
done

find "$STAGE" -type d -name "__pycache__" -prune -exec rm -rf {} +

(
  cd "$OUT_DIR"
  if command -v zip >/dev/null 2>&1; then
    zip -qr "fiap-$VERSION.zip" "fiap-$VERSION"
  else
    tar -czf "fiap-$VERSION.tar.gz" "fiap-$VERSION"
  fi
)

echo "release package: $ZIP"
