#!/usr/bin/env python3
"""Check actual independent GPU fixture screenshots; requires Pillow.

This verifies pixels, not their rendering provenance. Pair it with the NRO hash,
source/SDK profile and emulator event log. Never equate this with Switch hardware.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
from PIL import Image


def check(boot_path, after_path, kind, disabled_path=None):
    boot = Image.open(boot_path).convert('RGB')
    after = Image.open(after_path).convert('RGB')
    if boot.size != after.size:
        raise ValueError('Capture dimensions differ')
    width, height = boot.size
    corner, center = (width//20, height//20), (width//2, height//2)
    background = boot.getpixel(corner)
    pixels = lambda image: getattr(image, "get_flattened_data", image.getdata)()
    colors = Counter(pixels(boot))
    visible = 1 - colors[background]/(width*height)
    checks = {
        'fixture_pixels_visible': visible > 0.2,
        'a_changes_background': background != after.getpixel(corner),
        'center_remains_stable': boot.getpixel(center) == after.getpixel(center),
    }
    if kind == 'triangle':
        checks['interpolated_colors_visible'] = len(colors) > 100
    else:
        orange = lambda rgb: rgb[0] > 200 and rgb[1] > 100 and rgb[2] < 100
        near_pixels = sum(count for color, count in colors.items() if orange(color))
        checks['near_textured_triangle_visible'] = near_pixels > width*height*0.05
        if disabled_path:
            disabled = Image.open(disabled_path).convert('RGB')
            if disabled.size != boot.size:
                raise ValueError('Depth control dimensions differ')
            disabled_pixels = sum(1 for color in pixels(disabled) if orange(color))
            checks['depth_negative_control_hides_near_triangle'] = disabled_pixels == 0
    return {'kind': kind, 'dimensions': [width, height],
            'background_before': list(background), 'background_after': list(after.getpixel(corner)),
            'non_background_fraction': round(visible, 6), 'checks': checks,
            'passed': all(checks.values()), 'physical_switch_verified': False}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('boot', type=Path)
    parser.add_argument('after_a', type=Path)
    parser.add_argument('--kind', choices=['triangle','texture-depth'], required=True)
    parser.add_argument('--depth-disabled', type=Path)
    args = parser.parse_args()
    result = check(args.boot, args.after_a, args.kind, args.depth_disabled)
    print(json.dumps(result, indent=2, sort_keys=True))
    raise SystemExit(0 if result['passed'] else 5)
