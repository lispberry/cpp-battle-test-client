#!/usr/bin/env bash
# Configure, build and run the tests with a CMake preset.
#   tools/scripts/linux/test.sh [debug|release|asan]    (default: debug)
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

preset="${1:-debug}"
# <print> needs libstdc++ 14; Ubuntu 24.04's default g++ is 13.
if [[ -z "${CXX:-}" ]] && command -v g++-14 >/dev/null 2>&1; then
	export CXX=g++-14
fi
cmake --preset "$preset"
cmake --build --preset "$preset"
ctest --preset "$preset"
