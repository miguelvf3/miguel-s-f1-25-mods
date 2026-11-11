#!/usr/bin/env bash
# Off-game checks of the interest logic. Needs zig on PATH.
set -euo pipefail
cd "$(dirname "$0")"
out="${TMPDIR:-/tmp}/f2_interest_test.exe"
zig c++ -std=c++20 -Wall -Wextra -O1 interest_test.cpp -o "$out"
"$out"
