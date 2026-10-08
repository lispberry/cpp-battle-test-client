#!/usr/bin/env bash
# Configure, build and run the tests with a CMake preset.
#   tools/scripts/mac/test.sh [debug|release|asan]    (default: debug)
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

preset="${1:-debug}"
cmake --preset "$preset"
cmake --build --preset "$preset"
ctest --preset "$preset"
