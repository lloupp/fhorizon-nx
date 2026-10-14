"""Independent color references for the 12-cell GPU atlas (PPM or screenshot)."""
import argparse
import json
from PIL import Image

GREEN = (32, 64, 96, 159, 223)

def check(image, *, clear=False):
    image = image.convert('RGB')
    if image.width < 320 or image.height < 180:
        raise ValueError('atlas capture too small')
    mismatches=[]
    for phase in range(12):
        for x,expected in ((54,(255,0,0)),(160,(0,GREEN[phase%5],0)),(265,(255,0,0))):
            if clear:expected=(0,GREEN[phase%5],0)
            px=int(((phase%4)*320+x)*image.width/1280)
            py=int(((phase//4)*180+90)*image.height/540)
            actual=image.getpixel((px,py))
            if any(abs(a-b)>1 for a,b in zip(actual,expected)):
                mismatches.append(dict(phase=phase,x=x,expected=expected,actual=actual))
    return dict(passed=not mismatches,checked_samples=36,mismatches=mismatches,
                physical_switch_verified=False)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image');parser.add_argument('--clear',action='store_true')
    args=parser.parse_args();result=check(Image.open(args.image),clear=args.clear)
    print(json.dumps(result,indent=2));raise SystemExit(0 if result['passed'] else 1)
