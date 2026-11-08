# Tools

These scripts generate build headers, emulator captures and comparison data from
a user-supplied `BattleCity.nes`. No ROM, generated header or captured asset is
committed to the repository.

## Build headers

```sh
python3 tools/export_all.py --rom /path/to/BattleCity.nes --out build/generated
```

This runs the ROM-table exporter and four FCEUX captures, then writes
`stages.h`, `stage1.h`, `result_screen.h` and `gameover_screen.h` into
`build/generated`. FCEUX must support `--loadlua`.

Individual exporters accept explicit paths:

```sh
python3 tools/export_stages.py --rom ROM --out build/generated
python3 tools/export_reference.py --captures DIR --out build/generated/stage1.h [--png-dir DIR]
python3 tools/export_result.py --ppu DIR/result-ppu.bin --out build/generated/result_screen.h
python3 tools/export_gameover.py --ppu DIR/gameover-ppu.bin --out build/generated/gameover_screen.h
```

## Optional reference pixels

The `reference_pixels` test compares against PNGs captured from FCEUX. These
are not shipped. Regenerate the complete reference directory with one command:

```sh
python3 tools/capture_all_references.py \
  --rom /path/to/BattleCity.nes --out build/reference
```

The script captures into a fresh temporary directory, runs every capture
below, converts the `.gd` screenshots with `gd_to_png.py`, and creates
`sprites/*.png` plus the CSV evidence with `export_trace.py`. It writes the
exact scene names used by `compare_reference.py` (`title`,
`construction_blink`, `stage_select`, `tally117`, `tally2p34`, and so on).
Every expected scene and sprite crop is verified before the target directory
is updated, so a partial or failed capture returns non-zero and never reuses
old screenshots. The partial output directory is printed for inspection.

- `capture_reference.lua` (title/stage1)
- `capture_interstage.lua` (stage select)
- `capture_result.lua` (result template)
- `capture_gameover.lua` (brick GAME OVER)
- `capture_field_overlay.lua` (battlefield GAME OVER)
- `capture_pause.lua` (PAUSE)
- `capture_construction.lua` (title CONSTRUCTION cursor/editor)
- `capture_titlescroll.lua` (title scroll frames)
- `capture_tally.lua` with `BC_PLAYERS=1/2` (tally frames, including 117)
- `capture_tally_ticks.lua` (slots `$0313`/`$0314` request frames)
- `capture_trace.lua` (sprite frames and sound-event evidence)
- `capture_audio.lua` (isolated request durations)

Then configure:

```sh
cmake -S . -B build -DBATTLECITY_ROM=/path/to/BattleCity.nes \
  -DBATTLECITY_REFERENCE_TESTS=ON \
  -DBATTLECITY_REFERENCE_DIR=build/reference
```

Explicitly enabling the test fails at configure time when the ROM, Python 3 or
`title.png` is missing. The comparison also checks every required PNG, RAM
snapshot and sprite crop before rendering and reports the full missing list.

## ROM identification

`inspect_rom.py` prints the iNES header, mapper, hashes and data ranges. The
supported build hash is checked by `export_stages.py`.
