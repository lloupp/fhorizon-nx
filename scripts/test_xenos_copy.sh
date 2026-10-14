#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK="$ROOT/.local/upstream/pinyon-shift/thirdparty/shiftglue-sdk"
WORK="$ROOT/.local/xenos-copy-host"
mkdir -p "$WORK/include"
bash "$ROOT/scripts/prepare_pinyon_headers.sh" "$WORK/include"
"${CXX:-c++}" -std=c++23 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
 -I"$ROOT/src/graphics" -isystem "$WORK/include" -isystem "$SDK/include" "$ROOT/tests/test_xenos_copy.cpp" -o "$WORK/test"
ASAN_OPTIONS=detect_leaks=0 "$WORK/test"
