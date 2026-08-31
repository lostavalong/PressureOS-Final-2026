#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-rpi}"

cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/opt/pressureos
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

echo "Build complete: $BUILD_DIR/pressureos"
echo "Run: $BUILD_DIR/pressureos --windowed"
