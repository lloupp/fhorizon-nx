#!/usr/bin/env bash
# Build a separate deko3d archive. Installed SDK and official Eden stay untouched.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${DEVKITPRO:?Set DEVKITPRO first}"
BASE="$ROOT/.local/deko-eden"
SDK="$BASE/deko3d"
TOOLS="$BASE/dekotools"
SDK_PIN=350f2b00a3e76ecd4f00191f8c5d6544ffbcb9db
TOOLS_PIN=c15523cea963c04fad77a46f23c3a96d26a86ab9
for tool in git meson ninja bison flex c++; do command -v "$tool" >/dev/null; done
mkdir -p "$BASE"
checkout() {
    local directory="$1" url="$2" pin="$3"
    if [[ ! -d "$directory/.git" ]]; then
        git init "$directory"
        git -C "$directory" remote add origin "$url"
        git -C "$directory" fetch --depth 1 origin "$pin"
        git -C "$directory" checkout --detach FETCH_HEAD
    fi
    [[ "$(git -C "$directory" rev-parse HEAD)" == "$pin" ]] || { echo "Upstream pin mismatch: $directory" >&2; exit 2; }
}
checkout "$SDK" https://github.com/devkitPro/deko3d.git "$SDK_PIN"
checkout "$TOOLS" https://github.com/fincs/dekotools.git "$TOOLS_PIN"
if [[ ! -x "$TOOLS/build/dekomme" ]]; then
    meson setup "$TOOLS/build" "$TOOLS"
    ninja -C "$TOOLS/build" -j"${FHNX_BUILD_JOBS:-2}"
fi
export PATH="$TOOLS/build:$DEVKITPRO/tools/bin:$PATH"
if [[ ! -f "$SDK/debug/mme_macros.h" ]]; then
    make -C "$SDK" -j"${FHNX_BUILD_JOBS:-2}" lib/libdeko3dd.a
fi
if ! head -1 "$SDK/debug/mme_macros.h" | grep -q 'FHNX: explicit'; then
    cp "$SDK/debug/mme_macros.h" "$BASE/mme_macros.stock.h"
    python3 "$ROOT/tools/deko_macro_upload.py" "$SDK/debug/mme_macros.h"
    # Upstream's outer target depends on source directory timestamps.
    touch "$SDK/source"
    make -C "$SDK" -j"${FHNX_BUILD_JOBS:-2}" lib/libdeko3dd.a
fi
test -s "$SDK/lib/libdeko3dd.a"
printf 'Prepared GPU workaround archive: %s\n' "$SDK/lib/libdeko3dd.a"
