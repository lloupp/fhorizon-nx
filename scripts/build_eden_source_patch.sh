#!/usr/bin/env bash
# Optional heavy host build; separate from official Eden and the installed SDK.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SOURCE="${FHNX_EDEN_SOURCE_DIR:-$ROOT/.local/eden-stable-source}"
BUILD="${FHNX_EDEN_BUILD_DIR:-$ROOT/.local/eden-stable-build}"
PIN=58c1e20ee58efa3900ba616207d460886214480b
for tool in git cmake ninja c++ pkg-config glslangValidator; do command -v "$tool" >/dev/null; done
if [[ ! -d "$SOURCE/.git" && ! -f "$SOURCE/.git" ]]; then
    git init "$SOURCE"
    git -C "$SOURCE" remote add origin https://github.com/eden-emulator/mirror.git
    git -C "$SOURCE" fetch --depth 1 origin "$PIN"
    git -C "$SOURCE" checkout --detach FETCH_HEAD
fi
[[ "$(git -C "$SOURCE" rev-parse HEAD)" == "$PIN" ]] || { echo 'Eden source pin mismatch' >&2; exit 2; }
# A modern libva is required by bundled FFmpeg. For a host with older libva,
# pass an extracted official Eden shared/lib directory explicitly.
LINK_FLAGS="${FHNX_EDEN_LINK_FLAGS:-}"
if [[ -n "${FHNX_EDEN_LIBVA_DIR:-}" ]]; then
    VA_RUNTIME="$ROOT/.local/eden-build-va"
    mkdir -p "$VA_RUNTIME"
    for library in libva libva-drm libva-x11; do
        cp -L "$FHNX_EDEN_LIBVA_DIR/$library.so.2" "$VA_RUNTIME/$library.so.2"
        ln -sfn "$library.so.2" "$VA_RUNTIME/$library.so"
    done
    LINK_FLAGS="${LINK_FLAGS:+$LINK_FLAGS }-L$VA_RUNTIME -Wl,-rpath,$VA_RUNTIME"
fi
for PATCH in "$ROOT/patches/eden-0.2.1-macro-rebase.patch" "$ROOT/patches/eden-0.2.1-gcc13-warning.patch"; do
    if ! git -C "$SOURCE" apply --reverse --check "$PATCH" 2>/dev/null; then
        git -C "$SOURCE" apply --check "$PATCH"
        git -C "$SOURCE" apply "$PATCH"
    fi
done
cmake -S "$SOURCE" -B "$BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_EXE_LINKER_FLAGS="$LINK_FLAGS" \
    -DCMAKE_CXX_FLAGS_RELWITHDEBINFO='-O1 -g1 -DNDEBUG' \
    -DENABLE_QT=OFF -DENABLE_WEB_SERVICE=OFF -DENABLE_CUBEB=OFF \
    -DENABLE_LIBUSB=OFF -DYUZU_ROOM=OFF -DBUILD_TESTING=OFF \
    -DYUZU_USE_BUNDLED_SDL2=ON -DYUZU_USE_BUNDLED_FFMPEG=ON \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF -Dzstd_FORCE_BUNDLED=ON
cmake --build "$BUILD" --target eden-cli --parallel "${FHNX_BUILD_JOBS:-2}"
echo "Built modified Eden; runtime validation must be recorded separately: $BUILD/bin/eden-cli"
