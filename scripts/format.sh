#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format not found in PATH" >&2
  exit 1
fi

mapfile -t FILES < <(rg --files -g "*.hpp" -g "*.cpp" "$ROOT_DIR")
if [ ${#FILES[@]} -eq 0 ]; then
  echo "no C++ files found" >&2
  exit 0
fi

clang-format -i "${FILES[@]}"
