#!/usr/bin/env bash
# Formatting and clang-tidy checks; arguments go to tools/scripts/check.py (e.g. `tidy --all`, `format --fix`).
#   tools/scripts/linux/check.sh <format|tidy> [--all] [--fix]
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

if command -v uv >/dev/null 2>&1; then
	exec uv run tools/scripts/check.py "$@"
fi
exec python3 tools/scripts/check.py "$@"
