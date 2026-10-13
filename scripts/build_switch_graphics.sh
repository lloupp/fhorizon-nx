#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${DEVKITPRO:?Set DEVKITPRO to your devkitPro installation first}"
TEMPLATE="$DEVKITPRO/examples/switch/templates/application/Makefile"
[[ -f "$TEMPLATE" ]] || { echo "Install switch-examples, deko3d and uam" >&2; exit 2; }
WORK="$ROOT/.local/switch-graphics"
mkdir -p "$WORK/source" "$WORK/data"
cp "$TEMPLATE" "$WORK/Makefile"
cp "$ROOT"/switch/graphics/source/* "$WORK/source/"
"$DEVKITPRO/tools/bin/uam" -s vert -o "$WORK/data/triangle_vsh.bin" "$WORK/source/triangle_vsh.glsl"
"$DEVKITPRO/tools/bin/uam" -s frag -o "$WORK/data/color_fsh.bin" "$WORK/source/color_fsh.glsl"
make -C "$WORK" -j"${FHNX_BUILD_JOBS:-2}" TARGET=fhnx_graphics LIBS="-ldeko3dd -lnx" APP_TITLE="FHNX deko3d diagnostic" APP_AUTHOR=FHNX
test -s "$WORK/fhnx_graphics.nro"
printf 'Built (not hardware-validated): %s\n' "$WORK/fhnx_graphics.nro"
