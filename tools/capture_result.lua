-- Capture the zero-kill result screen PPU state used by export_result.py.
-- $80=0 keeps forcing the stage-clear condition while the tally is built.
FCEU.speedmode('maximum')
local root = debug.getinfo(1, 'S').source:sub(2):match('^(.*)/tools/[^/]+$') or '.'
local output = os.getenv('BATTLECITY_TRACE_DIR') or (root .. '/build/rom-trace')
for frame = 1, 1450 do
    local keys = {}
    if (frame >= 500 and frame < 505) or (frame >= 1000 and frame < 1005) then
        keys.start = true
    end
    joypad.set(1, keys)
    if frame >= 1200 then memory.writebyte(0x80, 0) end
    FCEU.frameadvance()
end
local f = assert(io.open(output .. '/result-ppu.bin', 'wb'))
for i = 0, 0x3fff do f:write(string.char(ppu.readbyte(i))) end
f:close()
local r = assert(io.open(output .. '/result-ram.bin', 'wb'))
for i = 0, 0x7ff do r:write(string.char(memory.readbyte(i))) end
r:close()
local g = assert(io.open(output .. '/result.gd', 'wb'))
g:write(gui.gdscreenshot())
g:close()
os.exit(0)
