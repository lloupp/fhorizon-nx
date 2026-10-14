import json
import sys
from PIL import Image

def check(capture):
    image=Image.open(capture).convert('RGB');w,h=image.size
    checks=dict(copied_region=image.getpixel((3*w//8,3*h//8))==(64,128,191),
                preserved_background=image.getpixel((7*w//8,7*h//8))==(64,32,128))
    return dict(checks=checks,passed=all(checks.values()),physical_switch_verified=False)
if __name__=='__main__':
    result=check(sys.argv[1]);print(json.dumps(result,indent=2));raise SystemExit(0 if result['passed'] else 5)
