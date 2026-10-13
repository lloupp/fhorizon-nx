#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${DEVKITPRO:?Set DEVKITPRO first}"
STAGE="${1:?Select device, queue, memory, copy, swapchain, triangle or texture-depth}"
case "$STAGE" in
    device) PROBE=1;;
    queue) PROBE=2;;
    memory) PROBE=3;;
    copy) PROBE=4;;
    swapchain|triangle|texture-depth) PROBE=0;;
    *) exit 2;;
esac
WORK="$ROOT/.local/gpu-probes/$STAGE"
mkdir -p "$WORK/source" "$WORK/data"
cp "$DEVKITPRO/examples/switch/templates/application/Makefile" "$WORK/Makefile"
DEFINES="-DFHNX_GPU_PROBE=$PROBE ${FHNX_GPU_DEFINES:-}"
if [[ "$PROBE" != 0 ]]; then
    cp "$ROOT/switch/gpu_minimal/main.c" "$WORK/source/main.c"
else
    cp "$ROOT"/switch/graphics/source/* "$WORK/source/"
    VERT=triangle_vsh.glsl
    FRAG=color_fsh.glsl
    if [[ "$STAGE" == texture-depth ]]; then
        VERT=textured_vsh.glsl; FRAG=textured_fsh.glsl
        DEFINES="$DEFINES -DFHNX_GPU_TEXTURE_DEPTH"
    fi
    "$DEVKITPRO/tools/bin/uam" -s vert -o "$WORK/data/triangle_vsh.bin" "$WORK/source/$VERT"
    "$DEVKITPRO/tools/bin/uam" -s frag -o "$WORK/data/color_fsh.bin" "$WORK/source/$FRAG"
    [[ "$STAGE" != swapchain ]] || DEFINES="$DEFINES -DFHNX_GPU_CLEAR_ONLY"
fi
LIBS="${FHNX_DEKO_LIBRARY:--ldeko3dd} -lnx -lm"
rm -f "$WORK/fhnx_gpu_$STAGE.elf" "$WORK/fhnx_gpu_$STAGE.nro"
make -C "$WORK" -j"${FHNX_BUILD_JOBS:-2}" TARGET="fhnx_gpu_$STAGE" DEFINES="$DEFINES" LIBS="$LIBS" APP_TITLE="FHNX GPU $STAGE probe" APP_AUTHOR=FHNX
test -s "$WORK/fhnx_gpu_$STAGE.nro"
