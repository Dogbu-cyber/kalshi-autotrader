#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

CLANG_FORMAT_BIN=""
if command -v clang-format >/dev/null 2>&1; then
  CLANG_FORMAT_BIN="clang-format"
elif command -v /opt/homebrew/opt/llvm/bin/clang-format >/dev/null 2>&1; then
  CLANG_FORMAT_BIN="/opt/homebrew/opt/llvm/bin/clang-format"
elif command -v /usr/local/opt/llvm/bin/clang-format >/dev/null 2>&1; then
  CLANG_FORMAT_BIN="/usr/local/opt/llvm/bin/clang-format"
else
  echo "clang-format not found in PATH" >&2
  exit 1
fi

FILES=()
if command -v rg >/dev/null 2>&1; then
  while IFS= read -r file; do
    FILES+=("$file")
  done < <(rg --files -g "*.hpp" -g "*.cpp" --glob '!build/**' --glob '!.git/**' "$ROOT_DIR")
else
  while IFS= read -r file; do
    FILES+=("$file")
  done < <(find "$ROOT_DIR" -type f \( -name "*.hpp" -o -name "*.cpp" \) \
    -not -path "$ROOT_DIR/build/*" -not -path "$ROOT_DIR/.git/*")
fi
if [ ${#FILES[@]} -eq 0 ]; then
  echo "no C++ files found" >&2
  exit 0
fi

"$CLANG_FORMAT_BIN" -i "${FILES[@]}"
