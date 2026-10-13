#!/usr/bin/env python3
import json
import sys
from PIL import Image

def check(a,b):
    a,b=[Image.open(p).convert('RGB') for p in (a,b)]
    if a.size!=b.size:raise ValueError('capture dimensions')
    w,h=a.size
    left=(w//4,h//2);right=(3*w//4,h//2)
    checks={
        'left_half_red_boot':a.getpixel(left) in [(127,0,0),(128,0,0)],
        'left_full_red_after_a':b.getpixel(left)==(255,0,0),
        'right_green_stable':a.getpixel(right)==b.getpixel(right)==(0,255,0),
        'background_stable':a.getpixel((20,20))==b.getpixel((20,20)),
    }
    return {'checks':checks,'passed':all(checks.values()),'physical_switch_verified':False}
if __name__=='__main__':
    r=check(*sys.argv[1:]);print(json.dumps(r,indent=2));raise SystemExit(0 if r['passed'] else 5)
