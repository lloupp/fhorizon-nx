#!/usr/bin/env bash
# Official pinned GCC Standard package, isolated from the existing PGO install.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[[ "$(uname -m)" == x86_64 ]] || exit 2
WORK="$ROOT/.local/eden-gcc"
URL=https://stable.eden-emu.dev/v0.2.1/Eden-Linux-v0.2.1-amd64-gcc-standard.AppImage
SHA256=2fae658397daf13c118082a3eb65d61a6519967b5e22e6667756baecf6000c5a
mkdir -p "$WORK" "$ROOT/.local/tools"
if [[ ! -f "$WORK/Eden.AppImage" ]];then
 curl --fail --location --max-time 180 "$URL" -o "$WORK/Eden.AppImage.download"
 mv "$WORK/Eden.AppImage.download" "$WORK/Eden.AppImage"
fi
printf '%s  %s\n' "$SHA256" "$WORK/Eden.AppImage" | sha256sum --check --status
chmod +x "$WORK/Eden.AppImage"
if [[ ! -x "$WORK/squashfs-root/bin/eden-cli" ]];then
 (cd "$WORK" && ./Eden.AppImage --appimage-extract > extract.log)
fi
cat > "$ROOT/.local/tools/eden-gcc-cli" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export SHARUN_ALLOW_SYS_VKICD="${SHARUN_ALLOW_SYS_VKICD:-1}"
exec "$ROOT/.local/eden-gcc/squashfs-root/bin/eden-cli" "$@"
SH
chmod +x "$ROOT/.local/tools/eden-gcc-cli"
"$ROOT/.local/tools/eden-gcc-cli" --version
