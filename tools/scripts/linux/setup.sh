#!/usr/bin/env bash
# One-time setup on Debian/Ubuntu: compiler and CMake, LLVM 23 from apt.llvm.org (clang-format, clang-tidy and the
# headers the sw-include-style plugin is built against), uv and lefthook, then installs the git hooks.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

llvm_version=23  # keep in sync with LLVM_MAJOR in tools/scripts/check.py

sudo apt-get update
sudo apt-get install -y build-essential g++-14 cmake git curl wget lsb-release gnupg software-properties-common python3

if ! command -v "clang-tidy-$llvm_version" >/dev/null 2>&1; then
	curl -fsSL https://apt.llvm.org/llvm.sh -o /tmp/llvm.sh
	sudo bash /tmp/llvm.sh "$llvm_version" all
	rm -f /tmp/llvm.sh
fi

if ! command -v uv >/dev/null 2>&1; then
	curl -LsSf https://astral.sh/uv/install.sh | sh
	export PATH="$HOME/.local/bin:$PATH"
fi
uv tool install lefthook
lefthook install
echo "Done. Build and test with: tools/scripts/linux/test.sh"
