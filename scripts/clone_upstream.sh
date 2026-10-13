#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BASE="$ROOT/.local/upstream"
TARGET="$BASE/pinyon-shift"
PIN=a72a92a5f215a32875386b824b8b6f0f34e1caf9
mkdir -p "$BASE"
if [[ -d "$TARGET/.git" ]]; then
  echo "Upstream already cloned. Not pulling/modifying it: $TARGET"
else
  git init "$TARGET"
  git -C "$TARGET" remote add origin https://github.com/arcanite24/pinyon-shift.git
  git -C "$TARGET" fetch --depth 1 origin "$PIN"
  git -C "$TARGET" checkout --detach FETCH_HEAD
fi
[[ "$(git -C "$TARGET" rev-parse HEAD)" == "$PIN" ]] || { echo "Existing upstream is not the probe's pinned revision; review it manually" >&2; exit 2; }
git -C "$TARGET" submodule update --init --depth 1 thirdparty/shiftglue-sdk
printf 'Upstream commit (record this in experiments): '
git -C "$TARGET" rev-parse HEAD
printf 'Source stays private to .local; no game assets included.\n'
