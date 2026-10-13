#!/usr/bin/env python3
"""Preserve MME RAM bytes while giving each macro entry an explicit upload.

Workaround for Eden 0.2.1's invalidated unordered_dense source reference.
This transforms generated dekomme headers, never game shaders/instructions.
"""
import argparse
from pathlib import Path
import re


def explicit_uploads(words):
    if len(words) < 5 or words[0] & 0xe000ffff != 0xa0000045 or words[1] != 0:
        raise ValueError("Expected a dekomme 3D instruction upload starting at zero")
    length = ((words[0] >> 16) & 0x1fff) - 1
    if length <= 0 or length + 4 > len(words):
        raise ValueError("Truncated macro instruction upload")
    code, tail = words[2:2+length], words[2+length:]
    if tail[0] & 0xe000ffff != 0xa0000047 or tail[1] != 0:
        raise ValueError("Expected dekomme entry table")
    entries = tail[2:]
    if len(entries) != ((tail[0] >> 16) & 0x1fff) - 1:
        raise ValueError("Entry table size mismatch")
    if not entries or 0 not in entries or any(offset >= length for offset in entries):
        raise ValueError("Macro entry outside uploaded code")
    result = []
    memory = {}
    for offset in sorted(set(entries)):
        suffix = code[offset:]
        header = (words[0] & ~0x1fff0000) | ((len(suffix) + 1) << 16)
        result += [header, offset, *suffix]
        memory.update(enumerate(suffix, offset))
    # All physical instruction addresses and entry positions remain identical.
    if [memory[i] for i in range(length)] != code:
        raise ValueError("Workaround changed MME RAM")
    return result + tail


def transform(text):
    marker = "// FHNX: explicit macro suffix uploads for Eden 0.2.1\n"
    if text.startswith(marker):
        return text
    begin = text.index("\n\t0x", text.index("static constexpr"))
    end = text.index("\n};", begin)
    words = [int(word, 16) for word in re.findall(r"0x[0-9A-Fa-f]+", text[begin:end])]
    output = explicit_uploads(words)
    return marker + text[:begin] + "\n" + "".join("\t0x%08X,\n" % w for w in output) + text[end:]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("header", type=Path)
    args = parser.parse_args()
    args.header.write_text(transform(args.header.read_text()))
