#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${DEVKITPRO:?Set DEVKITPRO first}"
VARIANT="${FHNX_ADAPTER_VARIANT:-positive}"
[[ "$VARIANT" =~ ^[a-zA-Z0-9_-]+$ ]] || exit 2
WORK="$ROOT/.local/adapter-$VARIANT"
mkdir -p "$WORK/source" "$WORK/include" "$WORK/data"
cp "$DEVKITPRO/examples/switch/templates/application/Makefile" "$WORK/Makefile"
printf '\nCXXFLAGS += -std=gnu++20 -fexceptions\n' >> "$WORK/Makefile"
cp "$ROOT"/switch/adapter/*.cpp "$WORK/source/"
cp "$ROOT"/switch/adapter/*.hpp "$ROOT/src/graphics/commands.hpp" "$WORK/include/"
"$DEVKITPRO/tools/bin/uam" -s vert -o "$WORK/data/adapter_vsh.bin" "$ROOT/switch/graphics/source/textured_vsh.glsl"
"$DEVKITPRO/tools/bin/uam" -s frag -o "$WORK/data/adapter_solid.bin" "$ROOT/switch/graphics/source/color_fsh.glsl"
"$DEVKITPRO/tools/bin/uam" -s frag -o "$WORK/data/adapter_sample.bin" "$ROOT/switch/graphics/source/textured_fsh.glsl"
make -C "$WORK" -j"${FHNX_BUILD_JOBS:-2}" TARGET="fhnx_adapter_$VARIANT" INCLUDES=include \
    DEFINES="${FHNX_ADAPTER_DEFINES:-}" LIBS="${FHNX_DEKO_LIBRARY:--ldeko3dd} -lnx -lm" APP_TITLE="FHNX command adapter" APP_AUTHOR=FHNX
test -s "$WORK/fhnx_adapter_$VARIANT.nro"
