#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${DEVKITPRO:?Set DEVKITPRO}"
OUT="${1:?Output directory}"
SDK="$ROOT/.local/upstream/pinyon-shift/thirdparty/shiftglue-sdk"
HOST="$ROOT/.local/xenos-shader-producer"
mkdir -p "$HOST/include" "$OUT"
bash "$ROOT/scripts/prepare_pinyon_headers.sh" "$HOST/include"
"${CXX:-c++}" -std=c++23 -Wall -Wextra -Werror -O2 \
 -I"$ROOT/src/graphics" -isystem "$HOST/include" -isystem "$SDK/include" \
 "$ROOT/tools/xenos_shader_translate.cpp" "$SDK/src/graphics/format/ucode.cpp" -o "$HOST/translate"
"$HOST/translate" --synthetic "$OUT"
for i in 0 1;do
 "$DEVKITPRO/tools/bin/uam" -s frag -o "$OUT/xenos_pixel${i}_fsh.bin" "$OUT/xenos_pixel${i}_fsh.glsl"
done
python3 - "$OUT" <<'PY'
from pathlib import Path
import sys,json,hashlib
out=Path(sys.argv[1]);manifest={'pinyon_commit':'a72a92a5f215a32875386b824b8b6f0f34e1caf9','shiftglue_commit':'1e8968f712824b97d4f4330c5a22389c4c8be1b5','stage':'pixel','constant_binding':1,'constant_count':4,'programs':[]}
header='#pragma once\n#include <array>\nnamespace fhnx::gpu::bundle {\n'
for i in range(2):
 base=out/f'xenos_pixel{i}';files=[base.with_suffix('.ucode.be'),out/f'xenos_pixel{i}_fsh.glsl',out/f'xenos_pixel{i}_fsh.bin']
 entry={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in files};manifest['programs'].append(entry)
 code=files[0].read_bytes();glsl=files[1].read_text()
 assert len(code)==48 and ')fhnx"' not in glsl and files[2].read_bytes()[:4]==b'DKSH'
 header+=f'inline constexpr std::array<unsigned char,48> program{i}={{'+','.join(map(str,code))+'};\n'
 header+=f'inline constexpr char glsl{i}[]=R"fhnx({glsl})fhnx";\n'
header+='}\n';(out/'xenos_shader_bundle.hpp').write_text(header);(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Generated original synthetic Xenos microcode -> checked GLSL -> UAM DKSH bundle')
PY
