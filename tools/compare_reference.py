#!/usr/bin/env python3
"""Compare controlled renders against captured FCEUX pixels (stdlib only)."""
import argparse
import json
import struct
import subprocess
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WIDTH, HEIGHT = 256, 240
SCENES = ('title', 'titlec', 'construction', 'construction_blink',
          'construction_edit', 'gameover', 'fieldoverlay', 'pause',
          'titlescroll51', 'titlescroll171', 'tally0', 'tally33', 'tally72',
          'tally81', 'tally117', 'tally120', 'tally177', 'tally204',
          'tally251', 'tally2p0', 'tally2p34', 'tally2p43', 'tally2p70',
          'tally2p109', 'tally2p145', 'tally2p193', 'tally2p286',
          'tally2p396', 'stage_select', 'stage1')
# (scene, emulator frame, x, y, width, height) derived from capture_trace.lua.
SPRITES = (('right', 1330, 88, 208, 16, 16), ('up', 1370, 88, 186, 16, 16),
           ('left', 1410, 80, 184, 16, 16), ('down', 1450, 80, 206, 16, 16),
           ('spawn', 1100, 80, 208, 16, 16), ('shield', 1200, 80, 208, 16, 16),
           ('enemy', 1200, 112, 40, 16, 16), ('bullet', 1500, 83, 124, 8, 16))


def required_reference_files():
    files = [f'{name}.png' for name in SCENES]
    files.append('stage1-ram.bin')
    files.extend(f'sprites/{frame}.png' for _, frame, *_ in SPRITES)
    return files


def read_png(path):
    data = path.read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError(f'Invalid PNG: {path}')
    pos, compressed = 8, bytearray()
    while pos < len(data):
        size = struct.unpack_from('>I', data, pos)[0]
        kind, payload = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + size]
        if kind == b'IHDR':
            if struct.unpack('>IIBBBBB', payload) != (WIDTH, HEIGHT, 8, 2, 0, 0, 0):
                raise ValueError('Expected 256x240 noninterlaced RGB PNG')
        if kind == b'IDAT':
            compressed.extend(payload)
        pos += size + 12
    raw = zlib.decompress(compressed)
    stride, pixels, previous = WIDTH * 3, bytearray(), bytearray(WIDTH * 3)
    if len(raw) != HEIGHT * (stride + 1):
        raise ValueError('Invalid scanline length')
    for y in range(HEIGHT):
        start = y * (stride + 1)
        mode, row = raw[start], bytearray(raw[start + 1:start + stride + 1])
        if mode > 4:
            raise ValueError('Invalid PNG filter')
        for x in range(stride):
            a, b, c = row[x - 3] if x >= 3 else 0, previous[x], previous[x - 3] if x >= 3 else 0
            if mode == 1:
                predictor = a
            elif mode == 2:
                predictor = b
            elif mode == 3:
                predictor = (a + b) // 2
            elif mode == 4:
                estimate = a + b - c
                predictor = min((a, b, c), key=lambda v: abs(estimate - v))
            else:
                predictor = 0
            row[x] = (row[x] + predictor) & 255
        pixels.extend(row)
        previous = row
    return pixels


def write_png(path, pixels):
    def chunk(kind, payload):
        return (struct.pack('>I', len(payload)) + kind + payload
                + struct.pack('>I', zlib.crc32(kind + payload)))
    scan = b''.join(b'\0' + pixels[y * WIDTH * 3:(y + 1) * WIDTH * 3] for y in range(HEIGHT))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'
                     + chunk(b'IHDR', struct.pack('>IIBBBBB', WIDTH, HEIGHT, 8, 2, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(scan)) + chunk(b'IEND', b''))


def compare(name, actual, output, reference):
    expected = read_png(reference / f'{name}.png')
    excluded = set()
    crop = (100, 102, 40, 24) if name == 'fieldoverlay' else ((96, 118, 48, 22) if name == 'pause' else None)
    if name == 'stage1':
        # Only static scenery/HUD is under test. OAM shadow identifies the
        # reference's dynamic sprite rectangles, including its shield/explosion.
        ram = (reference / 'stage1-ram.bin').read_bytes()
        for pos in range(0x200, 0x300, 4):
            y, _, _, x = ram[pos:pos + 4]
            if y >= 239:
                continue
            excluded.update(yy * WIDTH + xx
                            for yy in range(y + 1, min(y + 17, HEIGHT))
                            for xx in range(x, min(x + 8, WIDTH)))
    changed, diff = [], bytearray(WIDTH * HEIGHT * 3)
    for i in range(WIDTH * HEIGHT):
        if crop and not (crop[0] <= i % WIDTH < crop[0] + crop[2]
                         and crop[1] <= i // WIDTH < crop[1] + crop[3]):
            continue
        if i in excluded:
            diff[i * 3:i * 3 + 3] = b'\x40\x40\x40'
        elif actual[i * 3:i * 3 + 3] != expected[i * 3:i * 3 + 3]:
            changed.append((i % WIDTH, i // WIDTH))
            diff[i * 3:i * 3 + 3] = b'\xff\x00\xff'
    write_png(output / f'{name}-actual.png', actual)
    write_png(output / f'{name}-diff.png', diff)
    compared = crop[2] * crop[3] if crop else WIDTH * HEIGHT - len(excluded)
    return {'scene': name, 'compared_pixels': compared,
            'excluded_sprite_pixels': len(excluded), 'different_pixels': len(changed),
            'first_differences': changed[:12]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--renderer', required=True, type=Path)
    parser.add_argument('--reference', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    missing = [name for name in required_reference_files()
               if not (args.reference / name).is_file()]
    if missing:
        print(f'Missing {len(missing)} reference file(s) in {args.reference}:',
              file=sys.stderr)
        for name in missing:
            print('  ' + name, file=sys.stderr)
        print('Regenerate them from a user-supplied ROM with:\n'
              '  python3 tools/capture_all_references.py --rom ROM --out DIR',
              file=sys.stderr)
        return 2
    results = []
    for name in SCENES:
        ppm = args.output / f'{name}.ppm'
        subprocess.run([str(args.renderer.resolve()), name, str(ppm)], check=True)
        data = ppm.read_bytes()
        header = b'P6\n256 240\n255\n'
        if not data.startswith(header) or len(data) != len(header) + WIDTH * HEIGHT * 3:
            raise ValueError('Invalid renderer output')
        result = compare(name, data[len(header):], args.output, args.reference)
        results.append(result)
        print(json.dumps(result))
    # These crops are original emulator frames, not snapshots of our renderer.
    # Positions and animation phases come from capture_trace.lua's OAM trace.
    for name, frame, x, y, width, height in SPRITES:
        ppm = args.output / f'{name}.ppm'
        subprocess.run([str(args.renderer.resolve()), name, str(ppm)], check=True)
        actual = ppm.read_bytes().split(b'\n', 3)[3]
        expected = read_png(args.reference / 'sprites' / f'{frame}.png')
        changed = sum(actual[(yy * WIDTH + xx) * 3:(yy * WIDTH + xx) * 3 + 3]
                      != expected[(yy * WIDTH + xx) * 3:(yy * WIDTH + xx) * 3 + 3]
                      for yy in range(y, y + height) for xx in range(x, x + width))
        result = {'scene': name, 'reference_frame': frame, 'crop': [x, y, width, height],
                  'compared_pixels': width * height, 'different_pixels': changed}
        write_png(args.output / f'{name}-actual.png', actual)
        results.append(result)
        print(json.dumps(result))
    (args.output / 'report.json').write_text(json.dumps(results, indent=2) + '\n')
    return int(any(r['different_pixels'] for r in results))


if __name__ == '__main__':
    raise SystemExit(main())
