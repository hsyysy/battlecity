-- Capture the 1P/2P result tally animation at known relative frames.
-- Set BC_PLAYERS=2 for the two-player layout.  Scores and per-type kills are
-- injected at frame 1150, then $80=0 forces the stage-clear tally at 1200.
FCEU.speedmode('maximum')
local root = debug.getinfo(1, 'S').source:sub(2):match('^(.*)/tools/[^/]+$') or '.'
local output = os.getenv('BATTLECITY_TRACE_DIR') or (root .. '/build/rom-trace')
os.execute('mkdir -p ' .. output)
local players = tonumber(os.getenv('BC_PLAYERS') or '1')
local targets = players == 2 and {[0]=true,[34]=true,[43]=true,[70]=true,
        [109]=true,[145]=true,[193]=true,[286]=true,[396]=true}
    or {[0]=true,[33]=true,[72]=true,[81]=true,[120]=true,
        [177]=true,[204]=true,[251]=true}
local started, startframe = false, 0
for frame = 1, 2200 do
    local keys = {}
    if players == 2 and frame >= 400 and frame < 405 then keys.select = true end
    if (frame >= 500 and frame < 505) or (frame >= 1000 and frame < 1005) then keys.start = true end
    joypad.set(1, keys)
    if frame >= 1150 and frame < 1160 then
        for i = 0, 7 do memory.writebyte(0x73 + i, players == 2 and i + 1 or (i < 4 and i + 1 or 0)) end
        local p1 = {0,1,2,3,4,5,6}
        for i = 0, 6 do memory.writebyte(0x15 + i, p1[i+1]) end
        if players == 2 then
            local p2 = {0,0,6,7,8,9,0,0}
            for i = 0, 7 do memory.writebyte(0x1c + i, p2[i+1]) end
        end
    end
    if frame >= 1200 then memory.writebyte(0x80, 0) end
    FCEU.frameadvance()
    if not started then
        local ok = true
        for x = 8, 10 do
            if ppu.readbyte(0x2800 + 12 * 32 + x) ~= string.byte('PTS', x - 7) then ok = false end
        end
        if ok then started = true; startframe = frame end
    end
    if started then
        local rel = frame - startframe
        if targets[rel] then
            local prefix = output .. '/' .. players .. 'p-' .. rel
            local g = assert(io.open(prefix .. '.gd', 'wb'))
            g:write(gui.gdscreenshot()); g:close()
            local p = assert(io.open(prefix .. '-ppu.bin', 'wb'))
            for i = 0, 0x3fff do p:write(string.char(ppu.readbyte(i))) end
            p:close()
            local r = assert(io.open(prefix .. '-ram.bin', 'wb'))
            for i = 0, 0x7ff do r:write(string.char(memory.readbyte(i))) end
            r:close()
        end
    end
end
os.exit(0)
