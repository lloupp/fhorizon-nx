#!/usr/bin/env bash
# Linux integration test for our independent diagnostics only.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NRO="${1:?Usage: eden_input_smoke.sh compiled.nro eden-cli SMOKE|GRAPHICS}"
BINARY="${2:?Pass an installed official Eden SDL executable}"
PREFIX="${3:-SMOKE}"
[[ "$PREFIX" == SMOKE || "$PREFIX" == GRAPHICS ]] || exit 2
for tool in Xvfb xwininfo xdotool import python3; do command -v "$tool" >/dev/null; done
test -f "$NRO"
NAME="$(basename "$NRO" .nro)"
PROFILE="${FHNX_TEST_PROFILE:-}"
[[ -z "$PROFILE" || "$PROFILE" =~ ^[a-zA-Z0-9_-]+$ ]] || exit 2
WORK="$ROOT/.local/eden-tests/$NAME${PROFILE:+-$PROFILE}"
mkdir -p "$WORK/config/eden" "$WORK/data/eden" "$WORK/runtime" "$WORK/captures"
for directory in nand sdmc log load dump keys play_time; do mkdir -p "$WORK/data/eden/$directory"; done
export XDG_CONFIG_HOME="$WORK/config" XDG_DATA_HOME="$WORK/data" XDG_RUNTIME_DIR="$WORK/runtime"
# AppImage sharun needs this to expose a system Vulkan driver such as lavapipe.
export SHARUN_ALLOW_SYS_VKICD="${SHARUN_ALLOW_SYS_VKICD:-1}"
NUMBER="${FHNX_DISPLAY_NUMBER:-$((100 + RANDOM % 500))}"
export DISPLAY="127.0.0.1:$NUMBER"
Xvfb ":$NUMBER" -screen 0 1280x720x24 -ac -nolisten unix -listen tcp > "$WORK/xvfb.log" 2>&1 &
XPID=$!
RUNPID=
trap '[[ -z "$RUNPID" ]] || kill "$RUNPID" 2>/dev/null || true; kill "$XPID" 2>/dev/null || true; wait "$XPID" 2>/dev/null || true' EXIT
sleep 1
python3 "$ROOT/scripts/test_eden.py" --nro "$NRO" --binary "$BINARY" --seconds 90 \
    --expect-events "$PREFIX" --report "$WORK/report.json" --log "$WORK/eden.log" > "$WORK/runner-output.json" &
RUNPID=$!
WINDOW=
for attempt in $(seq 1 50); do
    # Window title searches can fail before SDL maps the window; inspect its real ID.
    WINDOW="$(xwininfo -root -tree 2>/dev/null | awk '/SDL_App/ {print $1; exit}' || true)"
    [[ -z "$WINDOW" ]] || break
    sleep 0.1
done
[[ -n "$WINDOW" ]] || { wait "$RUNPID"; echo "No SDL window" >&2; exit 5; }
sleep "${FHNX_SETTLE_SECONDS:-5}"
import -window root "$WORK/captures/boot.png"
xdotool windowfocus --sync "$WINDOW"
# Fresh SDL config: keyboard 'a' -> Switch A; 'm' -> Switch Plus.
for attempt in 1 2 3; do
    xdotool keydown a; sleep 0.6; xdotool keyup a; sleep 0.6
done
import -window root "$WORK/captures/after-a.png"
if [[ "${FHNX_TEST_ANALOG:-0}" == 1 ]];then
    # The observed fresh SDL config maps arrow keys to left-stick directions.
    xdotool keydown Right; sleep 1; xdotool keyup Right; sleep 1
    import -window root "$WORK/captures/after-analog.png"
fi
xdotool keydown m; sleep 0.6; xdotool keyup m
wait "$RUNPID"
RUNPID=
cat "$WORK/report.json"
printf 'Review captures: %s\n' "$WORK/captures"
