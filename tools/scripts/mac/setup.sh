#!/usr/bin/env bash
# One-time setup on macOS: CMake, LLVM (clang-format, clang-tidy and the headers the sw-include-style plugin is built
# against), uv and lefthook, then installs the git hooks.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

if ! xcode-select -p >/dev/null 2>&1; then
	echo "error: the Xcode Command Line Tools are required: xcode-select --install" >&2
	exit 1
fi
if ! command -v brew >/dev/null 2>&1; then
	echo "error: Homebrew is required: https://brew.sh" >&2
	exit 1
fi

brew install cmake llvm uv lefthook
lefthook install
echo "Done. Build and test with: tools/scripts/mac/test.sh"
