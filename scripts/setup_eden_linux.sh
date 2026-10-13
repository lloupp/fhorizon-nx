#!/usr/bin/env bash
# Pinned official Linux amd64 PGO package; installs only under ignored .local.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[[ "$(uname -m)" == x86_64 ]] || { echo "This package requires x86_64; use official downloads for other CPUs" >&2; exit 2; }
WORK="$ROOT/.local/eden"
URL=https://stable.eden-emu.dev/v0.2.1/Eden-Linux-v0.2.1-amd64-clang-pgo.AppImage
SHA256=7a28bf988b0648831989722bdbaa90ab31371b403808199813e0ea7c8b25ba6d
mkdir -p "$WORK" "$ROOT/.local/tools"
if [[ ! -f "$WORK/Eden.AppImage" ]]; then
    curl --fail --location --max-time 180 "$URL" -o "$WORK/Eden.AppImage.download"
    mv "$WORK/Eden.AppImage.download" "$WORK/Eden.AppImage"
fi
printf '%s  %s\n' "$SHA256" "$WORK/Eden.AppImage" | sha256sum --check --status
chmod +x "$WORK/Eden.AppImage"
if [[ ! -x "$WORK/squashfs-root/bin/eden-cli" ]]; then
    (cd "$WORK" && ./Eden.AppImage --appimage-extract > extract.log)
fi
cat > "$ROOT/.local/tools/eden-cli" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export SHARUN_ALLOW_SYS_VKICD="${SHARUN_ALLOW_SYS_VKICD:-1}"
exec "$ROOT/.local/eden/squashfs-root/bin/eden-cli" "$@"
SH
chmod +x "$ROOT/.local/tools/eden-cli"
"$ROOT/.local/tools/eden-cli" --version
