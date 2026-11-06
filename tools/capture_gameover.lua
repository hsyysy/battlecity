-- Capture the ROM's brick "GAME OVER" screen.
-- The game is started normally, then all remaining lives are cleared at frame
-- 1100.  The script waits for the ROM to present the large brick GAME OVER
-- nametable (rows 9-12 and 15-18 of $2000) and dumps that PPU state.
FCEU.speedmode('maximum')
local root = debug.getinfo(1, 'S').source:sub(2):match('^(.*)/tools/[^/]+$') or '.'
local output = os.getenv('BATTLECITY_TRACE_DIR') or (root .. '/build/rom-trace')
os.execute('mkdir -p ' .. output)
local function gameover_visible()
    for y = 0, 29 do
        local nonzero = false
        for x = 0, 31 do
            if ppu.readbyte(0x2000 + y * 32 + x) ~= 0 then nonzero = true end
        end
        local expected = (y >= 9 and y <= 12) or (y >= 15 and y <= 18)
        if nonzero ~= expected then return false end
    end
    return true
end
for frame = 1, 3000 do
    local keys = {}
    if (frame >= 500 and frame < 505) or (frame >= 1000 and frame < 1005) then
        keys.start = true
    end
    joypad.set(1, keys)
    if frame >= 1100 and frame < 1110 then memory.writebyte(0x51, 0) end
    FCEU.frameadvance()
    if frame > 1100 and gameover_visible() then
        -- Let the PPU enable rendering and settle on the new nametable.
        for _ = 1, 4 do FCEU.frameadvance() end
        local p = assert(io.open(output .. '/gameover-ppu.bin', 'wb'))
        for i = 0, 0x3fff do p:write(string.char(ppu.readbyte(i))) end
        p:close()
        local r = assert(io.open(output .. '/gameover-ram.bin', 'wb'))
        for i = 0, 0x7ff do r:write(string.char(memory.readbyte(i))) end
        r:close()
        local g = assert(io.open(output .. '/gameover.gd', 'wb'))
        g:write(gui.gdscreenshot()); g:close()
        os.exit(0)
    end
end
os.exit(1)
