#!/usr/bin/env bash
# Re-record e2e expected output (tests/e2e/.expected/). Review `git diff` before committing the result.
#   tools/scripts/mac/update-expected.sh           re-record every existing .expected file
#   tools/scripts/mac/update-expected.sh <name>    record tests/e2e/scenarios/<name>.txt, creating its file if needed
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

target="update_expected${1:+_$1}"
cmake --preset debug >/dev/null
cmake --build --preset debug --target "$target"
