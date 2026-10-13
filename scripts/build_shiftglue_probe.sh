#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${DEVKITPRO:?Set DEVKITPRO first}"
SDK="$ROOT/.local/upstream/pinyon-shift/thirdparty/shiftglue-sdk"
PIN=1e8968f712824b97d4f4330c5a22389c4c8be1b5
[[ "$(git -C "$SDK" rev-parse HEAD)" == "$PIN" ]] || { echo "ShiftGlue must match pinned revision $PIN" >&2; exit 2; }
MODE="${1:-headers}"
case "$MODE" in headers|context) ;; *) exit 2;; esac
WORK="$ROOT/.local/switch-runtime"
TARGET=fhnx_runtime
if [[ "$MODE" == context ]]; then
    WORK="$ROOT/.local/switch-context"
    TARGET=fhnx_context
    SIMDE="$SDK/thirdparty/simde"
    [[ "$(git -C "$SIMDE" rev-parse HEAD)" == 71fd833d9666141edcd1d3c109a80e228303d8d7 ]] || exit 2
fi
mkdir -p "$WORK/source" "$WORK/include/rex"
cp "$DEVKITPRO/examples/switch/templates/application/Makefile" "$WORK/Makefile"
printf '\nCXXFLAGS += -std=gnu++23 -fexceptions\n' >> "$WORK/Makefile"
cp "$ROOT/switch/source/main.c" "$WORK/source/main.c"
if [[ "$MODE" == context ]]; then
    ln -sfn "$SIMDE/simde" "$WORK/include/simde"
    mkdir -p "$WORK/include/rex/ppc"
    # Pinned upstream shares the x86 rounding decode table with ARM64.
    # ARM FPCR bits 01 mean +infinity, 10 mean -infinity (opposite of x86).
    python3 - "$SDK" "$WORK" <<'PYCTX'
from pathlib import Path
import sys
sdk, work = map(Path, sys.argv[1:])
source = (sdk / 'include/rex/ppc/context.h').read_text()
old = 'static constexpr size_t HostToGuest[] = {kRoundNearest, kRoundDown, kRoundUp, kRoundTowardZero};'
if source.count(old) != 1:
    raise SystemExit('Unexpected pinned FPSCR table; review required')
source = source.replace(old, 'static constexpr size_t HostToGuest[] = {kRoundNearest, kRoundUp, kRoundDown, kRoundTowardZero};')
(work / 'include/rex/ppc/context.h').write_text(source)
PYCTX
fi
if [[ "$MODE" == context ]]; then
    cp "$ROOT/switch/runtime/context_probe.cpp" "$WORK/source/"
else
    cp "$ROOT/switch/runtime/runtime_probe.cpp" "$WORK/source/"
fi
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
make -C "$WORK" -j"${FHNX_BUILD_JOBS:-2}" TARGET="$TARGET" INCLUDES=include LIBDIRS="$DEVKITPRO/libnx $SDK ${SIMDE:-}" DEFINES=-DFHNX_RUNTIME_PROBE APP_TITLE="FHNX ShiftGlue header probe" APP_AUTHOR=FHNX
test -s "$WORK/$TARGET.nro"
printf 'Built synthetic %s probe (not full runtime or hardware-validated): %s\n' "$MODE" "$WORK/$TARGET.nro"
