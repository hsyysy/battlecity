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
are not shipped. To regenerate a reference directory, run the relevant capture
scripts with `BATTLECITY_TRACE_DIR` pointing at it and convert the GD
screenshots. The scripts used by the checked-in validation are:

- `capture_reference.lua` (title/stage1)
- `capture_interstage.lua` (stage select)
- `capture_result.lua` (result template)
- `capture_gameover.lua` (brick GAME OVER)
- `capture_field_overlay.lua` (battlefield GAME OVER)
- `capture_pause.lua` (PAUSE)
- `capture_construction.lua` (title CONSTRUCTION cursor/editor)
- `capture_titlescroll.lua` (title scroll frames)
- `capture_tally.lua` with `BC_PLAYERS=1/2` (tally frames)
- `capture_tally_ticks.lua` (slots `$0313`/`$0314` request frames)
- `capture_trace.lua` (sprite frames and sound-event evidence)
- `capture_audio.lua` (isolated request durations)

`export_trace.py --input DIR --reference DIR` creates `sprites/*.png` and copies
the audio CSVs; `gd_to_png.py --input DIR --output DIR` converts the remaining
GD screenshots to PNG. Configure the test with `-DBATTLECITY_REFERENCE_TESTS=ON` and
`-DBATTLECITY_REFERENCE_DIR=/path/to/regenerated/references`.

## ROM identification

`inspect_rom.py` prints the iNES header, mapper, hashes and data ranges. The
supported build hash is checked by `export_stages.py`.
