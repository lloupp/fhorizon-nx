#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${DEVKITPRO:?Set DEVKITPRO first}"
STAGE="${1:?Select device, queue, memory, copy, swapchain, triangle, texture-depth, stencil, render-to-texture or resolve}"
case "$STAGE" in
    device) PROBE=1;;
    queue) PROBE=2;;
    memory) PROBE=3;;
    copy) PROBE=4;;
    swapchain|triangle|texture-depth|stencil|render-to-texture|resolve) PROBE=0;;
    *) exit 2;;
esac
VARIANT="${FHNX_GPU_VARIANT:-}"
[[ -z "$VARIANT" || "$VARIANT" =~ ^[a-zA-Z0-9_-]+$ ]] || exit 2
NAME="$STAGE${VARIANT:+-$VARIANT}"
TARGET="fhnx_gpu_$NAME"
WORK="$ROOT/.local/gpu-probes/$NAME"
mkdir -p "$WORK/source" "$WORK/data"
cp "$DEVKITPRO/examples/switch/templates/application/Makefile" "$WORK/Makefile"
DEFINES="-DFHNX_GPU_PROBE=$PROBE ${FHNX_GPU_DEFINES:-}"
if [[ "$PROBE" != 0 ]]; then
    cp "$ROOT/switch/gpu_minimal/main.c" "$WORK/source/main.c"
else
    cp "$ROOT"/switch/graphics/source/* "$WORK/source/"
    VERT=triangle_vsh.glsl
    FRAG=color_fsh.glsl
    if [[ "$STAGE" == texture-depth || "$STAGE" == stencil || "$STAGE" == render-to-texture || "$STAGE" == resolve ]]; then
        VERT=textured_vsh.glsl; FRAG=textured_fsh.glsl
        DEFINES="$DEFINES -DFHNX_GPU_TEXTURE_DEPTH"
        case "$STAGE" in
            stencil) DEFINES="$DEFINES -DFHNX_GPU_STENCIL";;
            render-to-texture) DEFINES="$DEFINES -DFHNX_GPU_RTT";;
            resolve) DEFINES="$DEFINES -DFHNX_GPU_RTT -DFHNX_GPU_RESOLVE";;
        esac
    fi
    "$DEVKITPRO/tools/bin/uam" -s vert -o "$WORK/data/triangle_vsh.bin" "$WORK/source/$VERT"
    "$DEVKITPRO/tools/bin/uam" -s frag -o "$WORK/data/color_fsh.bin" "$WORK/source/$FRAG"
    if [[ "$STAGE" == render-to-texture || "$STAGE" == resolve ]]; then
        "$DEVKITPRO/tools/bin/uam" -s frag -o "$WORK/data/solid_fsh.bin" "$WORK/source/color_fsh.glsl"
    fi
    [[ "$STAGE" != swapchain ]] || DEFINES="$DEFINES -DFHNX_GPU_CLEAR_ONLY"
fi
LIBS="${FHNX_DEKO_LIBRARY:--ldeko3dd} -lnx -lm"
rm -f "$WORK/$TARGET.elf" "$WORK/$TARGET.nro"
make -C "$WORK" -j"${FHNX_BUILD_JOBS:-2}" TARGET="$TARGET" DEFINES="$DEFINES" LIBS="$LIBS" APP_TITLE="FHNX GPU $STAGE probe" APP_AUTHOR=FHNX
test -s "$WORK/$TARGET.nro"
