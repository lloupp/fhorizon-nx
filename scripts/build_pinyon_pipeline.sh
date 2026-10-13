#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${DEVKITPRO:?Set DEVKITPRO first}"
SDK="$ROOT/.local/upstream/pinyon-shift/thirdparty/shiftglue-sdk"
VARIANT="${FHNX_PIPELINE_VARIANT:-}"
[[ -z "$VARIANT" || "$VARIANT" =~ ^[a-zA-Z0-9_-]+$ ]] || exit 2
TARGET="fhnx_pinyon_pipeline${VARIANT:+-$VARIANT}"
WORK="$ROOT/.local/pinyon-pipeline${VARIANT:+-$VARIANT}"
mkdir -p "$WORK/source" "$WORK/include" "$WORK/data"
cp "$DEVKITPRO/examples/switch/templates/application/Makefile" "$WORK/Makefile"
printf '\nCXXFLAGS += -std=gnu++23 -fexceptions\n' >> "$WORK/Makefile"
cp "$ROOT/switch/adapter/deko_backend.cpp" "$ROOT/switch/pinyon/main.cpp" "$WORK/source/"
cp "$ROOT/switch/adapter/deko_backend.hpp" "$ROOT/src/graphics/"*.hpp "$WORK/include/"
bash "$ROOT/scripts/prepare_pinyon_headers.sh" "$WORK/include"
"$DEVKITPRO/tools/bin/uam" -s vert -o "$WORK/data/pinyon_vsh.bin" "$ROOT/switch/graphics/source/textured_vsh.glsl"
"$DEVKITPRO/tools/bin/uam" -s frag -o "$WORK/data/pinyon_solid.bin" "$ROOT/switch/graphics/source/color_fsh.glsl"
"$DEVKITPRO/tools/bin/uam" -s frag -o "$WORK/data/pinyon_sample.bin" "$ROOT/switch/graphics/source/textured_fsh.glsl"
make -C "$WORK" -j"${FHNX_BUILD_JOBS:-2}" TARGET="$TARGET" INCLUDES=include LIBDIRS="$DEVKITPRO/libnx $SDK" \
 DEFINES="${FHNX_PIPELINE_DEFINES:-}" LIBS="${FHNX_DEKO_LIBRARY:--ldeko3dd} -lnx -lm" APP_TITLE="FHNX Pinyon surfaces" APP_AUTHOR=FHNX
test -s "$WORK/$TARGET.nro"
