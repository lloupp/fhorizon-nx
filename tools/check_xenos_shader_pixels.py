"""Independent final-frame colors for synthetic translated pixel programs."""
import argparse,json
from PIL import Image

def check(image):
    image=image.convert('RGB')
    if image.width<320 or image.height<180:raise ValueError('capture too small')
    refs=((54,(128,32,48)),(160,(64,64,48)),(265,(128,32,48)))
    checks={str(x):all(abs(a-b)<=1 for a,b in zip(image.getpixel((int(x*image.width/320),image.height//2)),expected)) for x,expected in refs}
    return dict(passed=all(checks.values()),checks=checks,physical_switch_verified=False)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('image');a=p.parse_args();r=check(Image.open(a.image));print(json.dumps(r,indent=2));raise SystemExit(0 if r['passed'] else 1)
