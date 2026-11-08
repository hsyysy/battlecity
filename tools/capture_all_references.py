#!/usr/bin/env python3
"""Regenerate all optional FCEUX reference pixels for reference_pixels.

The repository does not ship reference captures.  Run this once with your own
ROM and an FCEUX build that supports --loadlua:

    python3 tools/capture_all_references.py \
        --rom /path/to/BattleCity.nes --out build/reference

Captures are written into a fresh temporary directory.  The target directory is
updated only after every scene, RAM snapshot and sprite crop has been verified,
so a failed capture can never leave stale screenshots to be compared as if they
were new.
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from compare_reference import required_reference_files

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
RAW_FILES = (
    'title.gd', 'titlec.gd', 'construction.gd', 'construction_blink.gd',
    'construction_edit.gd', 'stage1.gd', 'stage1-ram.bin', 'gameover.gd',
    'fieldoverlay.gd', 'pause.gd', 'titlescroll51.gd', 'titlescroll171.gd',
    'stage_select.gd',
    'tally0.gd', 'tally33.gd', 'tally72.gd', 'tally81.gd', 'tally117.gd',
    'tally120.gd', 'tally177.gd', 'tally204.gd', 'tally251.gd',
    'tally2p0.gd', 'tally2p34.gd', 'tally2p43.gd', 'tally2p70.gd',
    'tally2p109.gd', 'tally2p145.gd', 'tally2p193.gd', 'tally2p286.gd',
    'tally2p396.gd',
    '1100.gd', '1200.gd', '1330.gd', '1370.gd', '1410.gd', '1450.gd',
    '1500.gd')


def missing_raw_files(directory):
    return [name for name in RAW_FILES if not (directory / name).is_file()]


def missing_final_files(directory):
    return [name for name in required_reference_files()
            if not (directory / name).is_file()]


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


def report_missing(kind, missing, temporary):
    print(f'Missing {len(missing)} {kind} file(s) after capture:',
          file=sys.stderr)
    for name in missing:
        print('  ' + name, file=sys.stderr)
    print(f'Partial captures kept in {temporary}', file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', required=True, type=Path,
                        help='user-supplied Battle City iNES ROM')
    parser.add_argument('--out', required=True, type=Path,
                        help='reference directory to create or refresh')
    parser.add_argument('--fceux', default='fceux',
                        help='FCEUX executable (default: fceux)')
    args = parser.parse_args()

    rom = args.rom.resolve()
    out = args.out.resolve()
    if not rom.is_file():
        parser.error(f'ROM not found: {rom}')
    if shutil.which(args.fceux) is None and not Path(args.fceux).is_file():
        parser.error(f'FCEUX not found: {args.fceux}')
    out.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix='battlecity-ref-', dir=out.parent))
    complete = False
    try:
        for script, extra in CAPTURES:
            capture(args.fceux, script, rom, temporary, extra)
        missing = missing_raw_files(temporary)
        if missing:
            report_missing('raw capture', missing, temporary)
            return 1
        subprocess.run([sys.executable, str(ROOT / 'tools/gd_to_png.py'),
                        '--input', str(temporary), '--output', str(temporary)],
                       check=True, timeout=120)
        subprocess.run([sys.executable, str(ROOT / 'tools/export_trace.py'),
                        '--input', str(temporary), '--reference', str(temporary)],
                       check=True, timeout=120)
        missing = missing_final_files(temporary)
        if missing:
            report_missing('reference', missing, temporary)
            return 1
        shutil.copytree(temporary, out, dirs_exist_ok=True)
        complete = True
        print(f'Reference pixels written to {out}')
        return 0
    except subprocess.CalledProcessError as error:
        print(f'Capture command failed with status {error.returncode}; '
              f'partial output kept in {temporary}', file=sys.stderr)
        return 1
    finally:
        if complete:
            shutil.rmtree(temporary, ignore_errors=True)


if __name__ == '__main__':
    raise SystemExit(main())
