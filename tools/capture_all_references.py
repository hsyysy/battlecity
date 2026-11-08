#!/usr/bin/env python3
"""Regenerate all optional FCEUX reference pixels for reference_pixels.

The repository does not ship reference captures.  Run this once with your own
ROM and an FCEUX build that supports --loadlua:

    python3 tools/capture_all_references.py \
        --rom /path/to/BattleCity.nes --out build/reference

The same directory can then be passed to CMake as BATTLECITY_REFERENCE_DIR and
is also where compare_reference.py looks for its scenes and sprite crops.
"""
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CAPTURES = (
    ('capture_reference.lua', {}),
    ('capture_construction.lua', {}),
    ('capture_interstage.lua', {}),
    ('capture_result.lua', {}),
    ('capture_gameover.lua', {}),
    ('capture_field_overlay.lua', {}),
    ('capture_pause.lua', {}),
    ('capture_titlescroll.lua', {}),
    ('capture_trace.lua', {}),
    ('capture_audio.lua', {}),
    ('capture_tally.lua', {'BC_PLAYERS': '1'}),
    ('capture_tally.lua', {'BC_PLAYERS': '2'}),
    ('capture_tally_ticks.lua', {'BC_PLAYERS': '1'}),
    ('capture_tally_ticks.lua', {'BC_PLAYERS': '2'}),
)


def capture(fceux, script, rom, out, extra):
    env = os.environ.copy()
    env.update({
        'BATTLECITY_TRACE_DIR': str(out),
        'FCEUX_CONFIG_DIR': str(out / 'fceux-config'),
        'QT_QPA_PLATFORM': 'offscreen',
        'SDL_VIDEODRIVER': 'dummy',
        'SDL_AUDIODRIVER': 'dummy',
    })
    env.update(extra)
    cmd = [fceux, '--no-config', '1', '--videoDriver', '1', '--sound', '0',
           '--loadlua', str(ROOT / 'tools' / script), str(rom)]
    print('+', ' '.join(cmd), flush=True)
    subprocess.run(cmd, check=True, env=env, timeout=600)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', required=True, type=Path,
                        help='user-supplied Battle City iNES ROM')
    parser.add_argument('--out', required=True, type=Path,
                        help='reference directory to create')
    parser.add_argument('--fceux', default='fceux',
                        help='FCEUX executable (default: fceux)')
    args = parser.parse_args()

    rom = args.rom.resolve()
    out = args.out.resolve()
    if not rom.is_file():
        parser.error(f'ROM not found: {rom}')
    if shutil.which(args.fceux) is None and not Path(args.fceux).is_file():
        parser.error(f'FCEUX not found: {args.fceux}')
    out.mkdir(parents=True, exist_ok=True)

    for script, extra in CAPTURES:
        capture(args.fceux, script, rom, out, extra)
    subprocess.run([sys.executable, str(ROOT / 'tools/gd_to_png.py'),
                    '--input', str(out), '--output', str(out)],
                   check=True, timeout=120)
    subprocess.run([sys.executable, str(ROOT / 'tools/export_trace.py'),
                    '--input', str(out), '--reference', str(out)],
                   check=True, timeout=120)
    print(f'Reference pixels written to {out}')


if __name__ == '__main__':
    main()
