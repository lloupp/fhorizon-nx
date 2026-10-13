#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${DEVKITPRO:?Set DEVKITPRO first}"
SDK="$ROOT/.local/upstream/pinyon-shift/thirdparty/shiftglue-sdk"
PIN=1e8968f712824b97d4f4330c5a22389c4c8be1b5
[[ "$(git -C "$SDK" rev-parse HEAD)" == "$PIN" ]] || { echo "ShiftGlue must match pinned revision $PIN" >&2; exit 2; }
WORK="$ROOT/.local/switch-runtime"
mkdir -p "$WORK/source" "$WORK/include/rex"
cp "$DEVKITPRO/examples/switch/templates/application/Makefile" "$WORK/Makefile"
printf '\nCXXFLAGS += -std=gnu++23 -fexceptions\n' >> "$WORK/Makefile"
cp "$ROOT/switch/source/main.c" "$WORK/source/main.c"
cp "$ROOT/switch/runtime/runtime_probe.cpp" "$WORK/source/"
cp "$ROOT/src/common/guest_memory.hpp" "$WORK/include/"
# Local OS detection overlay only; never claim Linux on Horizon, never patch upstream.
python3 - "$SDK" "$WORK" <<'PY'
from pathlib import Path
import sys
sdk, work = map(Path, sys.argv[1:])
source = (sdk / 'include/rex/platform.h').read_text()
needle = '#elif defined(__gnu_linux__)'
if source.count(needle) != 1:
    raise SystemExit('Unsupported upstream platform header; review required')
source = source.replace(needle, '#elif defined(__SWITCH__)\n#define REX_PLATFORM_SWITCH 1\n' + needle)
(work / 'include/rex/platform.h').write_text(source)
PY
make -C "$WORK" -j"${FHNX_BUILD_JOBS:-2}" TARGET=fhnx_runtime INCLUDES=include LIBDIRS="$DEVKITPRO/libnx $SDK" DEFINES=-DFHNX_RUNTIME_PROBE APP_TITLE="FHNX ShiftGlue header probe" APP_AUTHOR=FHNX
test -s "$WORK/fhnx_runtime.nro"
printf 'Built header/arena probe only (not full runtime or hardware-validated): %s\n' "$WORK/fhnx_runtime.nro"
