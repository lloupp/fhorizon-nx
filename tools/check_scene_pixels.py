#!/usr/bin/env python3
import json
import sys
from PIL import Image

def check(boot,after,analog,negative=None):
    images=[Image.open(p).convert('RGB') for p in (boot,after,analog)]
    if len({i.size for i in images[:3]})!=1:raise ValueError('capture dimensions')
    # The negative is an actual 480x270 GPU readback, saved as PPM by the NRO.
    # Normalize its resolution to the emulator window for spatial comparison.
    if negative:
        image=Image.open(negative).convert('RGB')
        images.append(image.resize(images[0].size,Image.Resampling.NEAREST))
    data=[list(i.get_flattened_data() if hasattr(i,'get_flattened_data') else i.getdata()) for i in images]
    total=len(data[0])
    blue=lambda c:c[2]>120 and c[2]>2*c[0] and c[2]>2*c[1]
    grass=lambda c:c[1]>40 and c[1]>2*c[0] and c[1]>2*c[2]
    blue_count=[sum(map(blue,d)) for d in data]
    diff=lambda a,b:sum(max(abs(x-y) for x,y in zip(c,d))>20 for c,d in zip(a,b))/total
    changes=[diff(data[0],data[1]),diff(data[1],data[2])]
    checks={'vehicle_blue_visible':blue_count[0]>50,
            'ground_visible':sum(map(grass,data[0]))/total>.03,
            'a_camera_changes_scene':changes[0]>.01,
            'analog_camera_changes_scene':changes[1]>.01}
    if negative:
        checks.update({'omitted_vehicle_no_blue':blue_count[3]==0,
                       'negative_ground_still_visible':sum(map(grass,data[3]))/total>.03,
                       'negative_preserves_scene_except_vehicle':diff(data[0],data[3])<.01})
    return {'checks':checks,'blue_pixels':blue_count,'camera_changed_fractions':changes,'passed':all(checks.values()),'negative_control_included':bool(negative),'physical_switch_verified':False}
if __name__=='__main__':
    r=check(*sys.argv[1:]);print(json.dumps(r,indent=2));raise SystemExit(0 if r['passed'] else 5)
