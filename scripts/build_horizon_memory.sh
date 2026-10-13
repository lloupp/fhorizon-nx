#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${DEVKITPRO:?Set DEVKITPRO first}"
MODE="${1:-pages}"
case "$MODE" in pages) NAME=memory;PROBE=memory_probe;;layout)NAME=layout;PROBE=memory_layout_probe;;*)exit 2;;esac
WORK="$ROOT/.local/horizon-$NAME"
mkdir -p "$WORK/source" "$WORK/include"
cp "$DEVKITPRO/examples/switch/templates/application/Makefile" "$WORK/Makefile"
printf '\nCXXFLAGS += -std=gnu++20 -fexceptions\n' >> "$WORK/Makefile"
cp "$ROOT/switch/source/main.c" "$WORK/source/"
cp "$ROOT/switch/runtime/horizon_memory.cpp" "$ROOT/switch/runtime/$PROBE.cpp" "$WORK/source/"
cp "$ROOT/switch/runtime/horizon_memory.hpp" "$WORK/include/"
make -C "$WORK" -j"${FHNX_BUILD_JOBS:-2}" TARGET="fhnx_horizon_$NAME" INCLUDES=include DEFINES=-DFHNX_RUNTIME_PROBE APP_TITLE="FHNX Horizon memory $MODE" APP_AUTHOR=FHNX
test -s "$WORK/fhnx_horizon_$NAME.nro"
