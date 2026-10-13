#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${DEVKITPRO:?Set DEVKITPRO first}"
WORK="$ROOT/.local/switch-framebuffer"
mkdir -p "$WORK/source"
cp "$DEVKITPRO/examples/switch/templates/application/Makefile" "$WORK/Makefile"
cp "$ROOT/switch/framebuffer/main.c" "$WORK/source/"
make -C "$WORK" -j"${FHNX_BUILD_JOBS:-2}" TARGET=fhnx_framebuffer APP_TITLE="FHNX CPU raster baseline" APP_AUTHOR=FHNX
test -s "$WORK/fhnx_framebuffer.nro"
