#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK="$ROOT/.local/upstream/pinyon-shift/thirdparty/shiftglue-sdk"
[[ "$(git -C "$SDK" rev-parse HEAD)" == 1e8968f712824b97d4f4330c5a22389c4c8be1b5 ]] || exit 2
OUT="${1:?Provide local include overlay directory}"
mkdir -p "$OUT/rex/graphics"
python3 - "$SDK" "$OUT" <<'PY'
from pathlib import Path
import sys
sdk,out=map(Path,sys.argv[1:])
source=(sdk/'include/rex/platform.h').read_text()
needle='#elif defined(__gnu_linux__)'
if source.count(needle)!=1:raise SystemExit('platform pin changed')
(out/'rex/platform.h').write_text(source.replace(needle,'#elif defined(__SWITCH__)\n#define REX_PLATFORM_SWITCH 1\n'+needle))
source=(sdk/'include/rex/graphics/xenos.h').read_text()
needle='#include <rex/memory.h>'
if source.count(needle)!=1:raise SystemExit('xenos dependency changed')
# The only memory names used by this exact pinned header are fourcc_t and
# make_fourcc, both defined in the real upstream utils.h. Avoid pulling the
# complete desktop Memory/MMIO/thread umbrella into this header-only boundary.
(out/'rex/graphics/xenos.h').write_text(source.replace(needle,'#include <rex/memory/utils.h>'))
PY
