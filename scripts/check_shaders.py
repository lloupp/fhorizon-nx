#!/usr/bin/env python3
"""Compile ORIGINAL synthetic GLSL to deko3d DKSH with UAM, if installed."""
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
uam = shutil.which('uam')
if not uam:
    print('SKIP: uam not installed. Shader inputs exist; DKSH compile not validated.')
    raise SystemExit(77)
out = root / '.local' / 'shaders'
out.mkdir(parents=True, exist_ok=True)
for stage, file in [('vert', 'triangle.vert'), ('frag', 'triangle.frag')]:
    dest = out / f'{file}.dksh'
    cmd = [uam, '-s', stage, '-o', str(dest), str(root / 'shaders' / file)]
    print('RUN:', ' '.join(cmd), flush=True)
    subprocess.run(cmd, check=True)
    if not dest.is_file() or dest.stat().st_size == 0:
        raise SystemExit(f'ERROR: no DKSH produced: {dest}')
print('PASS: synthetic GLSL -> DKSH (not an FH1 shader)')
