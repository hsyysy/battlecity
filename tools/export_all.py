#!/usr/bin/env python3
"""Generate every C header required by the game from a user-supplied ROM.

The repository intentionally does not contain BattleCity.nes or any exported
game data.  Run this once with your own legally obtained ROM:

    python3 tools/export_all.py --rom /path/to/BattleCity.nes --out build/generated

FCEUX with Lua support is required for the title/result/GAME OVER nametable
captures.  The generated headers are written into --out and are not input to
version control.
"""
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def run(cmd, env=None, timeout=180):
    print('+', ' '.join(str(part) for part in cmd), flush=True)
    subprocess.run(cmd, check=True, env=env, timeout=timeout)


def capture(fceux, script, rom, captures):
    env = os.environ.copy()
    env.update({
        'BATTLECITY_TRACE_DIR': str(captures),
        'FCEUX_CONFIG_DIR': str(captures / 'fceux-config'),
        'QT_QPA_PLATFORM': 'offscreen',
        'SDL_VIDEODRIVER': 'dummy',
        'SDL_AUDIODRIVER': 'dummy',
    })
    run([fceux, '--no-config', '1', '--videoDriver', '1', '--sound', '0',
         '--loadlua', str(ROOT / 'tools' / script), str(rom)], env=env)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', required=True, type=Path,
                        help='user-supplied Battle City iNES ROM')
    parser.add_argument('--out', required=True, type=Path,
                        help='directory for generated headers and captures')
    parser.add_argument('--fceux', default='fceux',
                        help='FCEUX executable (default: fceux)')
    args = parser.parse_args()

    rom = args.rom.resolve()
    out = args.out.resolve()
    if not rom.is_file():
        parser.error(f'ROM not found: {rom}')
    if shutil.which(args.fceux) is None:
        parser.error(f'FCEUX not found: {args.fceux} (needed for nametable captures)')
    out.mkdir(parents=True, exist_ok=True)
    captures = out / 'captures'
    captures.mkdir(parents=True, exist_ok=True)

    run([sys.executable, str(ROOT / 'tools/export_stages.py'),
         '--rom', str(rom), '--out', str(out)], timeout=60)
    for script in ('capture_reference.lua', 'capture_result.lua', 'capture_gameover.lua'):
        capture(args.fceux, script, rom, captures)
    run([sys.executable, str(ROOT / 'tools/export_reference.py'),
         '--captures', str(captures), '--out', str(out / 'stage1.h')], timeout=60)
    run([sys.executable, str(ROOT / 'tools/export_result.py'),
         '--ppu', str(captures / 'result-ppu.bin'),
         '--out', str(out / 'result_screen.h')], timeout=60)
    run([sys.executable, str(ROOT / 'tools/export_gameover.py'),
         '--ppu', str(captures / 'gameover-ppu.bin'),
         '--out', str(out / 'gameover_screen.h')], timeout=60)
    print(f'Generated headers in {out}')


if __name__ == '__main__':
    main()
