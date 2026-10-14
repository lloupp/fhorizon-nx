"""Explicit precision for a fresh Eden profile; never copy absolute user paths."""
import argparse
from pathlib import Path

ACCURACY = {'low': 0, 'medium': 1, 'high': 2}

def write_gpu_accuracy(path, value):
    if value not in ACCURACY:
        raise ValueError('gpu accuracy must be low, medium or high')
    path = Path(path)
    if path.exists():
        raise ValueError('accuracy override requires a fresh profile config')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(f'[Renderer]\ngpu_accuracy\\default=false\ngpu_accuracy={ACCURACY[value]}\n')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', required=True, type=Path)
    parser.add_argument('--gpu-accuracy', required=True, choices=ACCURACY)
    args = parser.parse_args()
    write_gpu_accuracy(args.config, args.gpu_accuracy)
