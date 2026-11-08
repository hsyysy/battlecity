#!/usr/bin/env python3
"""Refresh checked-in sprite screenshots from capture_trace.lua GD captures."""
import argparse
import shutil
from pathlib import Path
from compare_reference import ROOT, write_png


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, default=ROOT / 'build/rom-trace')
    parser.add_argument('--reference', type=Path, default=ROOT / 'assets/reference',
                        help='reference directory to receive sprite crops and CSV evidence')
    args = parser.parse_args()
    output = args.reference / 'sprites'
    output.mkdir(exist_ok=True)
    for frame in (1100, 1200, 1330, 1370, 1410, 1450, 1500):
        gd = (args.input / f'{frame}.gd').read_bytes()
        if gd[:7] != bytes.fromhex('ff fe 01 00 00 f0 01') or len(gd) != 11 + 256 * 240 * 4:
            raise ValueError('Unexpected GD truecolor screenshot format')
        rgb = b''.join(gd[i + 1:i + 4] for i in range(11, len(gd), 4))
        write_png(output / f'{frame}.png', rgb)
    for name in ('sound-durations.csv', 'sound-events.csv'):
        source = args.input / name
        if source.exists() and source.resolve() != (args.reference / name).resolve():
            shutil.copyfile(source, args.reference / name)
    print('Exported original ROM sprite frames and audio event evidence')


if __name__ == '__main__':
    main()
