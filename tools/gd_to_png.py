#!/usr/bin/env python3
"""Convert FCEUX .gd truecolor screenshots into RGB PNG files."""
import argparse
from pathlib import Path
from compare_reference import write_png


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True, type=Path,
                        help='directory containing .gd screenshots')
    parser.add_argument('--output', required=True, type=Path,
                        help='directory for .png files')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    count = 0
    for gd in sorted(args.input.glob('*.gd')):
        data = gd.read_bytes()
        if data[:7] != bytes.fromhex('ff fe 01 00 00 f0 01') or len(data) != 11 + 256 * 240 * 4:
            raise ValueError(f'Unexpected GD truecolor screenshot format: {gd}')
        rgb = b''.join(data[i + 1:i + 4] for i in range(11, len(data), 4))
        write_png(args.output / f'{gd.stem}.png', rgb)
        count += 1
    print(f'Converted {count} screenshot(s) to {args.output}')


if __name__ == '__main__':
    main()
