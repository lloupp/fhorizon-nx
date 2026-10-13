#!/usr/bin/env bash
# Host-only regression for Eden 0.2.1's invalidated macro-code reference.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$ROOT/.local/macro-rebase"
mkdir -p "$WORK/ankerl"
for file in unordered_dense.h stl.h; do
    [[ -s "$WORK/ankerl/$file" ]] || curl -fLsS "https://raw.githubusercontent.com/martinus/unordered_dense/v4.8.1/include/ankerl/$file" -o "$WORK/ankerl/$file"
done
(cd "$WORK/ankerl" && sha256sum -c <<'HASHES'
bc1174255ec5052ec6b82d69fb8e58b4d29615306996241df33f2c4cc47465a1  unordered_dense.h
954c0d08b6e14cc328dd1fdd359333f91a26546e6062c8b03c0b04056296e786  stl.h
HASHES
)
"${CXX:-c++}" -std=c++20 -O1 -g -fsanitize=address -fno-omit-frame-pointer -I"$WORK" "$ROOT/tools/diagnostics/macro_rebase_repro.cpp" -o "$WORK/repro"
if ASAN_OPTIONS=detect_leaks=0 "$WORK/repro" > "$WORK/original.log" 2>&1; then
    echo 'Original reproducer unexpectedly passed' >&2; exit 2
fi
grep -q 'heap-use-after-free' "$WORK/original.log"
ASAN_OPTIONS=detect_leaks=0 "$WORK/repro" fixed > "$WORK/fixed.log" 2>&1
grep -q 'REBASE_PASS words=7 first=2' "$WORK/fixed.log"
echo 'Original: ASan heap-use-after-free; copy-before-insert: PASS'
