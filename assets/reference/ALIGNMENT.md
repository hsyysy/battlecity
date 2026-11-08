# Pixel and sound reference scope

ROM SHA-256: `a869aead5b6957fc62002ff9636e048cc34baf0100d629b07dc51aa18f220c0c`.
Reference emulator: locally installed FCEUX 2.6.6, NTSC, no video filters.

The repository does not ship these ROM-derived captures. Regenerate the whole
reference directory with `tools/capture_all_references.py` as described in
`tools/README.md`, then enable `BATTLECITY_REFERENCE_TESTS`.

The FCEUX RGB values used by the PNG references are embedded in
`tools/export_reference.py`. They were derived from the locally installed
FCEUX palette: each 6-bit-aligned component is expanded with
`component | (component >> 6)`. Pixel comparisons confirm that these values
reproduce the captured reference colors exactly.

`reference_pixels` compares the whole title (61,440 pixels), including its
8x16-sprite cursor. It compares 60,512 pixels of stage one's scenery/HUD;
928 pixels covered by dynamic OAM sprite rectangles are explicitly excluded.
This is a controlled static state, not a replay of original frame 1300.
It also compares the full interstage/stage-select screen against
`stage_select.png` (captured by `tools/capture_interstage.lua` at frame 800);
the gray tile-$11 curtain, `STAGE` tiles and number rendering match exactly.
The title frame with the cursor on CONSTRUCTION and three full Construction
editor frames (cursor visible, cursor hidden, edited map) are compared the same
way; see the Construction section below.

All 35 stage layouts are decoded from PRG `$F07A` (36 × 91-byte nibble rows)
by `tools/export_stages.py`, together with the per-stage enemy composition at
`$E4EC`/`$E578`. The runtime eagle and its brick wall are added separately, as
in the original loader. A local FCEUX nametable capture of stages 2, 3, 16 and
35 was compared cell-by-cell against the decoded maps: the only differences
were bricks already destroyed by enemy fire by the capture frame. These extra
nametable captures are validation evidence only and are not checked in.

Construction mode follows the `Selected_Construction`, `Move_Tank` and
`Draw_TSABlock` routines in the annotated disassembly. The title has three
entries at `Tank_Y = $8B + CursorPos*16`; `titlec.png` is the FCEUX frame with
the cursor on CONSTRUCTION and the whole 256x240 frame matches. Entering the
editor calls `Make_GrayFrame` (tile `$11`, palette 0) and leaves a blank 26x26
field with the normal brick-walled eagle. The cursor starts at field cell 0,0
(centre 24,24) and moves in 16-pixel steps; the first direction press moves
immediately, a held direction waits 20 frames and then repeats every 5. A/B
paint the block under the cursor: their first press after movement paints the
current entry, later presses step forward/backward through TSA entries 0-$0D
(the last three entries are empty), and holding either button paints
continuously while the cursor moves. The 16-entry tile table and palette
assignments (`$D745` region) match `block_tiles` in `src/game.c`. The editor
uses the construction palette set, whose water sub-palette is `$3C/$10/$12`
rather than the gameplay `$3C/$12/$12`; water is static because the editor does
not run `Swap_Pal_Colors`. Because the construction loop runs
`TanksStatus_Handle` only when `Frame_Counter` bit 4 is set, the ROM cursor
blinks on for 16 frames and off for 16; the remake mirrors that with the
free-running frame counter.

`construction.png` is a stable FCEUX frame of the empty editor,
`construction_blink.png` is an empty-editor frame in the hidden half of the
blink cycle, and `construction_edit.png` is the final frame of the
deterministic timeline in `tools/capture_construction.lua` (movement, A/B
cycling, painting while moving, brick/steel/water/forest/ice blocks). All are
generated locally by `tools/capture_construction.lua` and the whole 256x240 frame matches pixel-for-pixel. Start stores
the edited map and returns
to the title with the CONSTRUCTION cursor; the next 1- or 2-player game uses
the custom map for one level, after which the ROM stage data is restored, as
`SetUp_LevelVARs` clears `Construction_Flag`. Spawning players and enemies
clear the 2x2 TSA block under them (`Make_Respawn`), so blocks built at a
spawn point are removed when the level begins. The map lives only in process
memory.

Player and bullet speeds in `src/game.c` come from the OAM trace: the player
moves three pixels per four frames, ordinary enemy tanks one pixel per two
frames, fast tanks one pixel per frame, and ordinary bullets two pixels per
frame. Enemy direction changes, spawn delay (`190 − stage × 4` frames), spawn
point rotation and spawn/shield durations follow the disassembly notes for
`$DB75`, `$DBF1`, `$DC7C`, `$E363` and `$E42B`. Random-AI trajectories are not
claimed to match a particular emulator run.

`sprites/*.png` are actual emulator screenshots produced by
`tools/capture_trace.lua`, using Start at frames 500 and 1000, then right at
1301–1340, up at 1341–1380, left at 1381–1420, down at 1421–1460, and up+A at
1461–1600. Selected crops validate player directions, basic enemies, spawn,
shield and an upward bullet. Screenshot frame N displays OAM prepared in frame
N−1; NES OAM Y is one less than the displayed sprite Y. These details are
reflected in `tests/render_reference.c`. The tests verify selected appearances,
not the animation cadence or motion trajectories.

Audio initialization at `$ea51`, update at `$ea7e`, sound request flags at
`$0300..$031b`, and stream pointers at `$ecfe` were identified in this ROM.
`sound-events.csv` records the original game's requests and their CPU PCs;
`sound-durations.csv` records isolated requests measured by
`tools/capture_audio.lua` for the original one-shot/engine events. The new
result-phase music durations come from the in-game traces described below,
because the title-screen driver leaves the music request flags set (it loops).

Runtime audio wraps the original PRG and sound routines in an in-memory NSF
and runs them through Game_Music_Emu, using separate voices for requests.
Shared-channel priority follows the original request ordering: among effects,
each event uses its lowest request slot id, so a lower-numbered slot wins a
shared channel. Music/engine beds yield a shared channel to a transient effect
for its duration and resume afterwards; a static bed-wins rule would silence
firing and ricochet effects for the whole battle-music phrase.
Each event's channel mask was checked by muting one libgme voice at a time:
stage-clear/record music uses pulse 1+2, the game-over jingle adds triangle,
and the tally tick uses pulse 2+noise. Mixing, phase, exact channel ownership
changes, NTSC/game tick conversion, and audio-device latency have not been
compared sample for sample with FCEUX.
Silence, deterministic PCM, audible output of the wired events, WAV output,
and game event routing have automated coverage. No claim of waveform identity
or complete sound coverage is made. The stage-clear routine at `$C47D` requests
slots 21/22/23, the game-over routine at `$C61B` requests slots 24/25/26, and
the tally count-up routine at `$CD2A` requests slots 19/20. Stage-clear and
game-over music are wired as `SOUND_STAGE_CLEAR` and `SOUND_GAMEOVER`; the
game-over jingle is held for the 114-frame brick screen (an in-game trace
measured 109 frames before the ROM's stop-all). The tally tick is wired as
`SOUND_TALLY`: a request-write trace during the 1P tally observed both $0313
and $0314 (slots 19/20) set at every kill decrement; the captured request
frames can be regenerated as `tally-ticks-1p.csv` and `tally-ticks-2p.csv`. In the wrapped driver the
isolated tick starts after two frames and produces a short pulse over the next
few frames, so the voice is kept alive for six frames instead of one. A
normal-speed FCEUX capture through SDL's disk audio driver shows the same tick
as an approximately 8-frame decaying pulse; libgme's shorter ~4-frame pulse is
an APU-envelope difference, so waveform identity is not claimed. The
power-up request at `$0309` (sequence slot 9, measured at 33 frames) is wired
through `SOUND_POWERUP`. Two-player audio is still not separately captured.

The result/tally screen is built from `result-ppu.bin`, a zero-kill FCEUX
capture of the screen the ROM writes to nametable $2800. Labels, attribute
palettes, separator, arrows and the OAM tank-icon positions were checked
against that capture; dynamic score/kill fields overwrite the captured zero
cells at the ROM's column positions. A request-write trace
(`tools/capture_tally_ticks.lua`) with per-type kills
1/2/3/4 established the exact cadence: the player score appears immediately,
each type row adds one kill every 9 frames, and the next type starts 30 frames
after the previous type's last entry. The type starts are 33/72/120/177 and
TOTAL appears at frame 251 (30+9 steps from the last entry plus the 8-frame
total delay). An earlier 66-frame 1P group reading came from sparse sampled
frames and is superseded by this trace. `tally0.png`, `tally33.png`,
`tally72.png`, `tally81.png`, `tally117.png`, `tally120.png`, `tally177.png`,
`tally204.png` and `tally251.png` are FCEUX frames regenerated locally and the
renderer matches all of them pixel-for-pixel; the TOTAL caption, separator and tank
icons appear with the first entry rather than on the initial PTS-label frame.
For a zero-kill loss the brick GAME OVER screen appears 303 frames after the
PTS frame; the internal zero-kill TOTAL frame is 161 and the fixed tail is 142
frames, used to recalibrate `TALLY_END_DELAY` from the old 65.

Two-player captures with P1 kills 1/2/3/4 and P2 kills 5/6/7/8 show a faster
schedule: the first entry is at frame 34, each shared step adds one entry per
column every 9 frames, type groups are 30 frames apart, and TOTAL appears at
frame 396. The 2P score line reproduces the ROM's layout: the left line is a
14-digit combined field (P1 at columns 5-10 and a zero-padded P2 field at
11-18), while the II-PLAYER column repeats the P2 score at columns 23-28.
`tally2p0.png`, `tally2p34.png`, `tally2p43.png`, `tally2p70.png`,
`tally2p109.png`, `tally2p145.png`, `tally2p193.png`, `tally2p286.png` and
`tally2p396.png` are FCEUX frames regenerated locally and all match
pixel-for-pixel.

The pause screen freezes the battlefield and adds five 8x16 sprites at OAM
Y=120, X=100/108/116/124/132, tiles $17/$19/$1B/$1D/$1F, sprite palette 3.
`pause.png` is the FCEUX frame and the local regression compares a 48x22
crop around the word pixel-for-pixel. The original Start key toggles pause;
this remake also accepts Enter and P.

The battlefield `GAME OVER` overlay is four 8x16 sprites at OAM Y=105,
X=104/112/120/128, tiles $79/$7B/$7D/$7F, sprite palette 3. FCEUX timing runs
(trigger at frames 1100/1200/1300, captured by
`tools/capture_field_overlay.lua`) place its first frame exactly 127 frames
after the loss and hold it for 130 frames; the field keeps running underneath.
`fieldoverlay.png` is the emulator frame and the local regression compares
the 40x24 text crop pixel-for-pixel. The implementation enters a `LOST_FIELD`
phase, keeps updating the world with player input suppressed, then switches to
the tally.

The brick `GAME OVER` screen is generated from `gameover-ppu.bin`, captured by
`tools/capture_gameover.lua` while the ROM presents it. The whole 256x240
frame is pixel-identical to the emulator capture. After the tally the ROM holds
this screen for about 114 frames, then scrolls the completed title nametable
upward at one pixel per frame; `titlescroll51.png` and `titlescroll171.png` are
emulator frames from that scroll and both match exactly. The selector tank
sprite appears only once the scroll reaches its final position.

The extra-life check follows $D13A: while the eagle is alive, each player
receives one extra life when the ten-thousands score digit reaches 2 (20,000
points). The ROM marks the award with an internal flag and requests sound
slots 4 and 5, which map to the existing enemy/player sound events. High score
is kept in RAM for the current process and is folded into the title and tally
HI-SCORE digits.

Two-player mode uses the ROM's P2 spawn centre at screen x=152 (stored as
field x=128), sprite palette 1, six enemy slots 2–7 and the 20-frame shorter
spawn delay. The 2P HUD adds the `IIP` row and second life counter exactly as
captured from the emulator; a controlled P2 tank crop is pixel-identical to
the FCEUX frame-1300 capture.

Power-up state follows `$E8BE`/`$E9E2`/`$E972`: flashing carriers are spawned
when the remaining-enemy counter is 17, 10 or 3; a kill drops one of six items
at a ROM grid position (`48/96/144/192` screen coordinates, stored here as
field-relative centres); pickup awards 500 points and applies helmet, clock,
shovel, star, grenade or extra-life. The icon tile range `$81 + type*4` and
its OAM offsets were checked against an emulator capture, and the rendered
icon is pixel-identical for a controlled stage-one state. Effect durations
(640/640/1280 frames), the 4px star bullets and the 3-star steel-piercing
bullet follow the disassembly notes.
