-- Capture the original PAUSE overlay.  Start begins the game, Start during
-- play freezes the field and adds five 8x16 sprites at OAM Y=120.
FCEU.speedmode('maximum')
local root = debug.getinfo(1, 'S').source:sub(2):match('^(.*)/tools/[^/]+$') or '.'
local output = os.getenv('BATTLECITY_TRACE_DIR') or (root .. '/build/rom-trace')
os.execute('mkdir -p ' .. output)
for frame = 1, 1400 do
    local keys = {}
    if (frame >= 500 and frame < 505) or (frame >= 1000 and frame < 1005) then
        keys.start = true
    end
    if frame >= 1250 and frame < 1255 then keys.start = true end
    joypad.set(1, keys)
    FCEU.frameadvance()
    if frame == 1300 then
        local g = assert(io.open(output .. '/pause.gd', 'wb'))
        g:write(gui.gdscreenshot()); g:close()
        local p = assert(io.open(output .. '/pause-ppu.bin', 'wb'))
        for i = 0, 0x3fff do p:write(string.char(ppu.readbyte(i))) end
        p:close()
        local r = assert(io.open(output .. '/pause-ram.bin', 'wb'))
        for i = 0, 0x7ff do r:write(string.char(memory.readbyte(i))) end
        r:close()
    end
end
os.exit(0)
