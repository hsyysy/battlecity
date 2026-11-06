-- Capture the battlefield "GAME OVER" sprite overlay.
-- Clearing the remaining lives makes the ROM present four 8x16 sprites
-- (tiles $79/$7B/$7D/$7F) 127 frames later for 130 frames.
FCEU.speedmode('maximum')
local root = debug.getinfo(1, 'S').source:sub(2):match('^(.*)/tools/[^/]+$') or '.'
local output = os.getenv('BATTLECITY_TRACE_DIR') or (root .. '/build/rom-trace')
os.execute('mkdir -p ' .. output)
local first, last, black = nil, nil, nil
local dumped = false
for frame = 1, 1700 do
    local keys = {}
    if (frame >= 500 and frame < 505) or (frame >= 1000 and frame < 1005) then
        keys.start = true
    end
    joypad.set(1, keys)
    if frame >= 1100 and frame < 1110 then memory.writebyte(0x51, 0) end
    FCEU.frameadvance()
    local found = false
    for i = 0, 63 do
        local y = memory.readbyte(0x200 + i * 4)
        local t = memory.readbyte(0x201 + i * 4)
        if y == 105 and (t == 0x79 or t == 0x7b or t == 0x7d or t == 0x7f) then
            found = true
        end
    end
    if found then
        if not first then first = frame end
        last = frame
        if not dumped and last - first >= 30 then
            local g = assert(io.open(output .. '/fieldoverlay.gd', 'wb'))
            g:write(gui.gdscreenshot()); g:close()
            local p = assert(io.open(output .. '/fieldoverlay-ppu.bin', 'wb'))
            for i = 0, 0x3fff do p:write(string.char(ppu.readbyte(i))) end
            p:close()
            local r = assert(io.open(output .. '/fieldoverlay-ram.bin', 'wb'))
            for i = 0, 0x7ff do r:write(string.char(memory.readbyte(i))) end
            r:close()
            dumped = true
        end
    end
    if frame > 1100 and memory.readbyte(0x6b) == 1 and not black then black = frame end
end
local f = assert(io.open(output .. '/fieldoverlay-timing.txt', 'w'))
f:write(string.format('first=%d last=%d length=%d black=%d\n',
    first or -1, last or -1, first and (last - first + 1) or 0, black or -1))
f:close()
os.exit(dumped and first and black and 0 or 1)
