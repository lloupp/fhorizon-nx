#!/usr/bin/env python3
"""FHNX lightweight preflight. Never executes, decrypts or copies game files."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

XEX_NAMES = ('default.xex', 'XMediaFacade_default.xex', 'SpeechFacade_default.xex')
DENIED_EXTENSIONS = {'.xex', '.iso', '.xgd', '.dvd', '.nsp', '.xci', '.nca', '.xexp', '.xcp', '.tik', '.key', '.keys', '.pfx', '.p12', '.pem', '.dksh'}
DENIED_DIRECTORIES = {'.local', 'game', 'games', 'extracted', 'generated', 'saves', 'cache', 'romfs', 'build', '.git'}


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def doctor() -> dict:
    tools = ['python3', 'cmake', 'ctest', 'c++', 'clang++', 'ninja', 'git', 'aarch64-none-elf-gcc', 'elf2nro', 'uam', 'spirv-cross', 'eden-cli', 'eden']
    devkit = os.getenv('DEVKITPRO')
    return {
        'purpose': 'Host/build tool inventory; not a game compatibility verdict',
        'host_arch': platform.machine(),
        'host_system': platform.system(),
        'tools': {t: shutil.which(t) for t in tools},
        'devkitpro': devkit if devkit else None,
        'switch_toolchain_ready': bool(devkit and (Path(devkit) / 'libnx' / 'switch_rules').exists() and shutil.which('aarch64-none-elf-gcc')),
    }


def inspect_game(root: Path) -> dict:
    """Read only; inspect XEX magic and hashes; never assume the disc revision."""
    if not root.exists():
        raise FileNotFoundError(root)
    result: dict = {'input_type': 'directory' if root.is_dir() else 'file', 'title_id_expected': '4D5309C9',
                    'verified_revision': False, 'gameplay_tested': False}
    if root.is_file():
        result['format_hint'] = root.suffix.lower()
        result['bytes'] = root.stat().st_size
        result['note'] = 'ISO/package identification requires upstream verified manifest; no game contents read'
        return result
    records = []
    for name in XEX_NAMES:
        p = root / name
        if not p.is_file() or p.is_symlink():
            records.append({'name': name, 'present': False})
            continue
        with p.open('rb') as fd:
            magic = fd.read(4)
        records.append({'name': name, 'present': True, 'xex2_magic': magic == b'XEX2',
                        'bytes': p.stat().st_size, 'sha256': sha256_file(p)})
    result['modules'] = records
    result['all_expected_modules_present'] = all(m['present'] and m.get('xex2_magic') for m in records)
    result['note'] = 'Magic and SHA256 do not verify an official disc revision. Compare against a trusted upstream manifest.'
    return result


def audit_public_tree(root: Path) -> dict:
    """Fail closed on likely proprietary or generated binaries before publishing."""
    issues = []
    root = root.resolve()
    for current, dirs, files in os.walk(root, followlinks=False):
        dirs[:] = [d for d in dirs if d not in {'.git', '__pycache__', '.local', 'build', 'out'}]
        relative = Path(current).relative_to(root)
        if any(segment.lower() in DENIED_DIRECTORIES for segment in relative.parts):
            issues.append(str(relative) + '/ (private/generated directory)')
            dirs[:] = []
            continue
        for name in files:
            p = Path(current) / name
            if p.is_symlink():
                issues.append(str(p.relative_to(root)) + ' (symlink: review manually)')
            elif p.suffix.lower() in DENIED_EXTENSIONS:
                issues.append(str(p.relative_to(root)) + ' (blocked extension)')
    return {'safe_for_review': not issues, 'issues': issues,
            'note': 'Conservative audit only. Manual review and upstream legal policy remain mandatory.'}


def emit(data: dict, output: Path | None) -> None:
    text = json.dumps(data, indent=2, ensure_ascii=False, sort_keys=True) + '\n'
    if output is not None:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(text, encoding='utf-8')
    print(text, end='')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    d = sub.add_parser('doctor', help='Show installed build tools; no game required')
    d.add_argument('--out', type=Path)
    i = sub.add_parser('inspect', help='Read-only inventory of your locally extracted game')
    i.add_argument('path', type=Path)
    i.add_argument('--out', type=Path)
    a = sub.add_parser('audit', help='Check a proposed public source tree for prohibited files')
    a.add_argument('path', type=Path)
    a.add_argument('--out', type=Path)
    args = parser.parse_args()
    try:
        if args.action == 'doctor':
            value = doctor()
        elif args.action == 'inspect':
            value = inspect_game(args.path)
        else:
            value = audit_public_tree(args.path)
        emit(value, args.out)
        return 2 if args.action == 'audit' and not value['safe_for_review'] else 0
    except (OSError, ValueError) as exc:
        print(f'ERROR: {exc}', file=sys.stderr)
        return 1

if __name__ == '__main__':
    raise SystemExit(main())
