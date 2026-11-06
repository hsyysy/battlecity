-- Trace the tally count-up sound requests (ROM slots 19/20) relative to the
-- first PTS frame.  Set BC_PLAYERS=2 for the two-player schedule.  The script
-- uses the same score/kill injection as capture_tally.lua.
FCEU.speedmode('maximum')
local root = debug.getinfo(1, 'S').source:sub(2):match('^(.*)/tools/[^/]+$') or '.'
local output = os.getenv('BATTLECITY_TRACE_DIR') or (root .. '/build/rom-trace')
os.execute('mkdir -p ' .. output)
local players = tonumber(os.getenv('BC_PLAYERS') or '1')
local log = assert(io.open(output .. '/tally-ticks-' .. players .. 'p.csv', 'w'))
log:write('relative_frame,slot\n')
local frame = 0
local started, startframe = false, 0
local function register(addr)
    memory.registerwrite(addr, 1, function(_, _, value)
        if value == 1 and started then
            log:write(frame - startframe, ',', addr, '\n')
        end
    end)
end
register(0x313)
register(0x314)
for f = 1, 2600 do
    frame = f
    local keys = {}
    if players == 2 and f >= 400 and f < 405 then keys.select = true end
    if (f >= 500 and f < 505) or (f >= 1000 and f < 1005) then keys.start = true end
    joypad.set(1, keys)
    if f >= 1150 and f < 1160 then
        for i = 0, 7 do
            memory.writebyte(0x73 + i, players == 2 and i + 1 or (i < 4 and i + 1 or 0))
        end
        local p1 = {0,1,2,3,4,5,6}
        for i = 0, 6 do memory.writebyte(0x15 + i, p1[i+1]) end
        if players == 2 then
            local p2 = {0,0,6,7,8,9,0,0}
            for i = 0, 7 do memory.writebyte(0x1c + i, p2[i+1]) end
        end
    end
    if f >= 1200 then memory.writebyte(0x80, 0) end
    FCEU.frameadvance()
    if not started then
        local ok = true
        for x = 8, 10 do
            if ppu.readbyte(0x2800 + 12 * 32 + x) ~= string.byte('PTS', x - 7) then ok = false end
        end
        if ok then started = true; startframe = f end
    end
end
log:close()
os.exit(0)
