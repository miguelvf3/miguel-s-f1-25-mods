#!/usr/bin/env bash
# Needs a SwoftyLauncher checkout next to this folder (../SwoftyLauncher) and zig.
set -euo pipefail
cd "$(dirname "$(readlink -f "$0")")"
REPO=${SWOFTY_REPO:-../../SwoftyLauncher}
export ZIG_GLOBAL_CACHE_DIR=$PWD/build/zig-cache ZIG_LOCAL_CACHE_DIR=$PWD/build/zig-cache
mkdir -p build
zig c++ -target x86_64-windows-gnu -std=c++23 -O2 -Wall -Wextra -Wno-nullability-completeness \
    -I"$REPO/native/include" -I"$REPO/packages/sdk/include" -D__USE_MINGW_ANSI_STDIO=1 -shared -s \
    -o f2_feeder.dll src/f2_feeder.cpp
rm -f f2_feeder.lib
echo "built $(pwd)/f2_feeder.dll"
