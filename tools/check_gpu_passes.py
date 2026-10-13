#!/usr/bin/env python3
"""Check new GPU pass pixels against an executed negative control (Pillow)."""
import argparse
import json
from collections import Counter
from pathlib import Path
from PIL import Image


def check(boot_path, after_path, negative_path, kind):
    images = [Image.open(p).convert('RGB') for p in (boot_path, after_path, negative_path)]
    boot, after, negative = images
    if not boot.size == after.size == negative.size:
        raise ValueError('Capture dimensions differ')
    w, h = boot.size
    corner, center = (w//20, h//20), (w//2, h//2)
    colors = [Counter(i.get_flattened_data() if hasattr(i, 'get_flattened_data') else i.getdata()) for i in images]
    bg = boot.getpixel(corner)
    foreground = [1 - c[bg]/(w*h) for c in (colors[0], colors[2])]
    checks = {
        'a_changes_background': bg != after.getpixel(corner),
        'center_stable_after_a': boot.getpixel(center) == after.getpixel(center),
        'negative_changes_fixture': boot.getpixel(center) != negative.getpixel(center),
    }
    if kind == 'stencil':
        # Same far triangle, constrained by a smaller stencil mask in the pass.
        checks['mask_has_visible_pixels'] = 0.10 < foreground[0] < 0.25
        checks['disabled_stencil_expands_coverage'] = foreground[1] > 2*foreground[0]
        # At the far triangle tip, only the negative control draws.
        tip = (w//2, h//10)
        checks['outside_mask_clipped'] = boot.getpixel(tip) == bg and negative.getpixel(tip) != bg
        # Center has the same far primitive in both: stencil constrains coverage.
        checks.pop('negative_changes_fixture')
    else:
        orange = lambda c: c[0] > 220 and 100 < c[1] < 160 and c[2] < 20
        white = lambda c: min(c) > 240
        green = lambda c: c[1] > 140 and c[0] < 20 and c[2] < 20
        fraction = lambda counter, predicate: sum(n for c, n in counter.items() if predicate(c))/(w*h)
        checks['offscreen_clear_sampled'] = fraction(colors[0], orange) > 0.08
        checks['offscreen_draw_sampled'] = fraction(colors[0], white) > 0.02
        checks['skipped_pass_has_no_orange_or_white'] = fraction(colors[2], orange)+fraction(colors[2], white) == 0
        checks['skipped_pass_keeps_green_destination'] = fraction(colors[2], green) > 0.10
    return {'kind': kind, 'dimensions': [w, h], 'foreground_fractions': foreground,
            'checks': checks, 'passed': all(checks.values()), 'physical_switch_verified': False}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('boot', 'after_a', 'negative'): parser.add_argument(name, type=Path)
    parser.add_argument('--kind', choices=['stencil', 'render-to-texture', 'resolve'], required=True)
    args = parser.parse_args()
    result = check(args.boot, args.after_a, args.negative, args.kind)
    print(json.dumps(result, indent=2, sort_keys=True))
    raise SystemExit(0 if result['passed'] else 5)
