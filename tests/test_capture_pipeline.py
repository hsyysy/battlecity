#!/usr/bin/env python3
"""Check that failed reference captures never refresh an existing directory."""
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import capture_all_references as capture_all


def main():
    with tempfile.TemporaryDirectory() as directory:
        directory = Path(directory)
        missing = capture_all.missing_raw_files(directory)
        if missing != list(capture_all.RAW_FILES):
            print('Empty capture directory did not report every raw file',
                  file=sys.stderr)
            return 1
        for name in capture_all.RAW_FILES:
            if name == 'tally251.gd':
                continue
            path = directory / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'')
        if capture_all.missing_raw_files(directory) != ['tally251.gd']:
            print('Partial capture was not reported precisely', file=sys.stderr)
            return 1

        for name in capture_all.required_reference_files():
            if name == 'sprites/1330.png':
                continue
            path = directory / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'')
        if capture_all.missing_final_files(directory) != ['sprites/1330.png']:
            print('Missing final reference file was not reported', file=sys.stderr)
            return 1

    with tempfile.TemporaryDirectory() as directory:
        directory = Path(directory)
        rom = directory / 'dummy.nes'
        rom.write_bytes(b'')
        fake_fceux = directory / 'fceux'
        fake_fceux.write_text('#!/bin/sh\nexit 0\n')
        fake_fceux.chmod(0o755)
        target = directory / 'reference'
        target.mkdir()
        stale = target / 'tally117.png'
        stale.write_bytes(b'old reference')

        result = subprocess.run(
            [sys.executable, str(ROOT / 'tools/capture_all_references.py'),
             '--rom', str(rom), '--out', str(target),
             '--fceux', str(fake_fceux)],
            capture_output=True, text=True)
        if result.returncode == 0:
            print('Empty capture unexpectedly succeeded', file=sys.stderr)
            return 1
        if stale.read_bytes() != b'old reference':
            print('Failed capture modified the existing reference directory',
                  file=sys.stderr)
            return 1
        if 'missing' not in (result.stdout + result.stderr).lower():
            print('Failure did not report the missing capture files',
                  file=sys.stderr)
            return 1

    lua = shutil.which('lua')
    if lua:
        for script in ('capture_tally.lua', 'capture_titlescroll.lua'):
            with tempfile.TemporaryDirectory() as directory:
                env = os.environ.copy()
                env['BATTLECITY_TRACE_DIR'] = directory
                result = subprocess.run(
                    [lua, str(ROOT / 'tests/capture_failure_stub.lua'),
                     str(ROOT / 'tools' / script)],
                    env=env, capture_output=True, text=True, timeout=60)
                if result.returncode == 0:
                    print(f'{script} reported success without target frames',
                          file=sys.stderr)
                    return 1
                if 'missing' not in result.stderr.lower():
                    print(f'{script} did not list missing target frames',
                          file=sys.stderr)
                    return 1
    else:
        print('SKIP: lua not found; capture-script fault injection not run')

    print('PASS: failed captures are reported and never refresh stale references')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
