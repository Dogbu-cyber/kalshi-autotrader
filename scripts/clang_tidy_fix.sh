#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ ! -f "$ROOT_DIR/compile_commands.json" ]; then
  echo "compile_commands.json not found; run ./scripts/build.sh first" >&2
  exit 1
fi

CLANG_TIDY_BIN=""
if command -v run-clang-tidy >/dev/null 2>&1; then
  CLANG_TIDY_BIN="run-clang-tidy"
elif command -v /opt/homebrew/opt/llvm/bin/run-clang-tidy >/dev/null 2>&1; then
  CLANG_TIDY_BIN="/opt/homebrew/opt/llvm/bin/run-clang-tidy"
else
  echo "run-clang-tidy not found" >&2
  exit 1
fi

"$CLANG_TIDY_BIN" -p "$ROOT_DIR" -fix -fix-errors
