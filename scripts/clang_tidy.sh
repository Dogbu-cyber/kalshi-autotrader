#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

if [[ ! -d "$BUILD_DIR" ]]; then
  echo "build directory not found: $BUILD_DIR" >&2
  exit 1
fi

CLANG_TIDY=""
RUN_CLANG_TIDY=""
EXTRA_ARGS=()

if [[ -x /opt/homebrew/opt/llvm/bin/run-clang-tidy ]]; then
  RUN_CLANG_TIDY="/opt/homebrew/opt/llvm/bin/run-clang-tidy"
fi

if [[ -x /opt/homebrew/opt/llvm/bin/clang-tidy ]]; then
  CLANG_TIDY="/opt/homebrew/opt/llvm/bin/clang-tidy"
  SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
  EXTRA_ARGS=(--extra-arg=-isysroot --extra-arg="$SDKROOT")
elif command -v clang-tidy >/dev/null 2>&1; then
  CLANG_TIDY="$(command -v clang-tidy)"
fi

if [[ -n "$RUN_CLANG_TIDY" && -n "$CLANG_TIDY" ]]; then
  "$RUN_CLANG_TIDY" -p "$BUILD_DIR" -j "${1:-8}" -clang-tidy "$CLANG_TIDY" \
    -extra-arg=-isysroot -extra-arg="$SDKROOT"
  exit 0
fi

if [[ -z "$CLANG_TIDY" ]]; then
  echo "clang-tidy not found. Install with: brew install llvm" >&2
  exit 1
fi

FILES=$(python3 - <<'PY'
import json
from pathlib import Path

db = Path("build/compile_commands.json")
data = json.loads(db.read_text())
files = sorted({entry["file"] for entry in data})
print(" ".join(files))
PY
)

if [[ -z "$FILES" ]]; then
  echo "no files found in compile_commands.json" >&2
  exit 1
fi

"$CLANG_TIDY" -p "$BUILD_DIR" "${EXTRA_ARGS[@]}" $FILES
