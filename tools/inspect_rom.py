#!/usr/bin/env python3
"""Inspect an iNES NROM and export both CHR banks, with no dependencies."""
import argparse
import hashlib
import json
import struct
import zlib
from pathlib import Path


def png(path, width, height, pixels):
    def chunk(kind, payload):
        return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload))
    scan = b''.join(b'\0' + bytes(pixels[y * width:(y + 1) * width]) for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 0, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(scan)) + chunk(b'IEND', b''))


def inspect(rom, out):
    data = rom.read_bytes()
    if len(data) < 16 or data[:4] != b'NES\x1a':
        raise ValueError('Not an iNES ROM')
    h = data[:16]
    if (h[7] & 12) == 8:
        raise ValueError('NES 2.0 is not supported by this tool')
    prg_size, chr_size = h[4] * 16384, h[5] * 8192
    start = 16 + (512 if h[6] & 4 else 0)
    if len(data) != start + prg_size + chr_size or prg_size < 6:
        raise ValueError('Invalid or truncated ROM layout')
    prg, chr_data = data[start:start + prg_size], data[start + prg_size:]
    nmi, reset, irq = struct.unpack('<HHH', prg[-6:])
    report = dict(file=rom.name, sha256=hashlib.sha256(data).hexdigest(),
                  mapper=(h[6] >> 4) | (h[7] & 240), prg_bytes=prg_size,
                  chr_bytes=chr_size, mirroring='vertical' if h[6] & 1 else 'horizontal',
                  vectors=dict(nmi=f'0x{nmi:04X}', reset=f'0x{reset:04X}', irq=f'0x{irq:04X}'))
    out.mkdir(parents=True, exist_ok=True)
    (out / 'rom.json').write_text(json.dumps(report, indent=2) + '\n')
    for bank in range(chr_size // 4096):
        # 16 x 16 tiles per bank, four times native resolution.
        pixels = bytearray(512 * 512)
        for tile in range(256):
            off = bank * 4096 + tile * 16
            for y in range(8):
                for x in range(8):
                    value = ((chr_data[off + y] >> (7 - x)) & 1) | (((chr_data[off + y + 8] >> (7 - x)) & 1) << 1)
                    for sy in range(4):
                        for sx in range(4):
                            pixels[((tile // 16 * 8 + y) * 4 + sy) * 512 + (tile % 16 * 8 + x) * 4 + sx] = (0, 100, 180, 255)[value]
        png(out / f'chr-bank{bank}.png', 512, 512, pixels)
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', nargs='?', type=Path, default=Path('BattleCity.nes'))
    parser.add_argument('--out', type=Path, default=Path('assets/reference'))
    args = parser.parse_args()
    inspect(args.rom, args.out)
