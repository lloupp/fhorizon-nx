#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK="$ROOT/.local/upstream/pinyon-shift/thirdparty/shiftglue-sdk"
WORK="$ROOT/.local/xenos-shader-host"
mkdir -p "$WORK/include"
bash "$ROOT/scripts/prepare_pinyon_headers.sh" "$WORK/include"
"${CXX:-c++}" -std=c++23 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
 -I"$ROOT/src/graphics" -isystem "$WORK/include" -isystem "$SDK/include" \
 "$ROOT/tests/test_xenos_shader.cpp" "$SDK/src/graphics/format/ucode.cpp" -o "$WORK/test"
ASAN_OPTIONS=detect_leaks=0 "$WORK/test" "$WORK"
