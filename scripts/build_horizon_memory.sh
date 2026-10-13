#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${DEVKITPRO:?Set DEVKITPRO first}"
WORK="$ROOT/.local/horizon-memory"
mkdir -p "$WORK/source" "$WORK/include"
cp "$DEVKITPRO/examples/switch/templates/application/Makefile" "$WORK/Makefile"
printf '\nCXXFLAGS += -std=gnu++20 -fexceptions\n' >> "$WORK/Makefile"
cp "$ROOT/switch/source/main.c" "$WORK/source/"
cp "$ROOT/switch/runtime/"{horizon_memory.cpp,memory_probe.cpp} "$WORK/source/"
cp "$ROOT/switch/runtime/horizon_memory.hpp" "$WORK/include/"
make -C "$WORK" -j"${FHNX_BUILD_JOBS:-2}" TARGET=fhnx_horizon_memory INCLUDES=include DEFINES=-DFHNX_RUNTIME_PROBE APP_TITLE="FHNX sparse Horizon memory" APP_AUTHOR=FHNX
test -s "$WORK/fhnx_horizon_memory.nro"
