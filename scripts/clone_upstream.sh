#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BASE="$ROOT/.local/upstream"
TARGET="$BASE/pinyon-shift"
mkdir -p "$BASE"
if [[ -d "$TARGET/.git" ]]; then
  echo "Upstream already cloned. Not pulling/modifying it: $TARGET"
else
  git clone --filter=blob:none https://github.com/arcanite24/pinyon-shift.git "$TARGET"
fi
printf 'Upstream commit (record this in experiments): '
git -C "$TARGET" rev-parse HEAD
printf 'Source stays private to .local; no game assets included.\n'
