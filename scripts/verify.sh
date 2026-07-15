#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ -f "$ROOT_DIR/frontend/package.json" ]]; then
  npm --prefix "$ROOT_DIR/frontend" ci
  npm --prefix "$ROOT_DIR/frontend" run build
fi

cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT_DIR/build" --parallel
ctest --test-dir "$ROOT_DIR/build" --output-on-failure
