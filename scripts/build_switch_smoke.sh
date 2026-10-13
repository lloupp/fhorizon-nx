#!/usr/bin/env bash
set -euo pipefail
# No assumption that the devkitPro toolchain is installed.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${DEVKITPRO:?Set DEVKITPRO to your devkitPro installation first}"
TEMPLATE="${DEVKITPRO}/examples/switch/templates/application"
if [[ ! -f "$TEMPLATE/Makefile" ]]; then
  echo "Missing official libnx application template: $TEMPLATE" >&2
  exit 2
fi
WORK="$ROOT/.local/switch-smoke"
mkdir -p "$WORK/source"
cp "$TEMPLATE/Makefile" "$WORK/Makefile"
cp "$ROOT/switch/source/main.c" "$WORK/source/main.c"
# Use project name so output is fhnx_smoke.nro.
sed -i.bak 's/TARGET[[:space:]]*:=[[:space:]]*$(notdir $(CURDIR))/TARGET := fhnx_smoke/' "$WORK/Makefile"
rm -f "$WORK/Makefile.bak"
make -C "$WORK" -j2
printf 'Built (not hardware-validated): %s\n' "$WORK/fhnx_smoke.nro"
