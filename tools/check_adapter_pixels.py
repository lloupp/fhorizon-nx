#!/usr/bin/env python3
"""Compare adapter presentation against executed resolve/stencil controls."""
import argparse
import json
from collections import Counter
from PIL import Image

def check(boot, after, no_resolve, no_stencil):
    images=[Image.open(p).convert('RGB') for p in (boot,after,no_resolve,no_stencil)]
    if len({i.size for i in images})!=1:
        raise ValueError('capture sizes differ')
    w,h=images[0].size
    counts=[Counter(i.get_flattened_data() if hasattr(i,'get_flattened_data') else i.getdata()) for i in images]
    def fraction(n,predicate):
        return sum(v for k,v in counts[n].items() if predicate(k))/(w*h)
    blue=lambda c: c[2]>230 and c[0]<80 and 50<c[1]<110
    orange=lambda c: c[0]>240 and 110<c[1]<150 and c[2]<20
    green=lambda c: c[1]>140 and c[0]<20 and c[2]<20
    checks={
        'a_background_changed':images[0].getpixel((20,20))!=images[1].getpixel((20,20)),
        'center_stable':images[0].getpixel((w//2,h//2))==images[1].getpixel((w//2,h//2)),
        'rendered_blue_sampled':fraction(0,blue)>0.03,
        'orange_clear_sampled':fraction(0,orange)>0.1,
        'omitted_resolve_keeps_green':fraction(2,green)>0.14,
        'omitted_resolve_has_no_fixture':fraction(2,blue)+fraction(2,orange)==0,
        # The first pass is sampled inside a second triangle, so its screen
        # coverage ratio differs from the old directly presented stencil test.
        'outside_mask_orange':orange(images[0].getpixel((w//2,3*h//10))),
        'outside_mask_blue_without_stencil':blue(images[3].getpixel((w//2,3*h//10))),
        'disabled_stencil_expands_blue':fraction(3,blue)-fraction(0,blue)>0.02,
        'disabled_stencil_reduces_orange':fraction(0,orange)-fraction(3,orange)>0.02,
    }
    return {'checks':checks,'passed':all(checks.values()),'physical_switch_verified':False}

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('boot','after','no_resolve','no_stencil'):p.add_argument(name)
    r=check(**vars(p.parse_args())); print(json.dumps(r,indent=2)); raise SystemExit(0 if r['passed'] else 5)
