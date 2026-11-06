-- Measure isolated original sound requests while the title screen is idle.
-- Output directory must already exist. This does not alter the ROM on disk.
FCEU.speedmode('maximum')
local root = debug.getinfo(1, 'S').source:sub(2):match('^(.*)/tools/[^/]+$') or '.'
local output = os.getenv('BATTLECITY_TRACE_DIR') or (root .. '/build/rom-trace')
local definitions = {
    {'start',1,2,3}, {'fire',15}, {'brick',12}, {'steel',13},
    {'enemy',10}, {'player',7}, {'base',7,11}, {'pause',0}, {'move',17}, {'idle',18},
    {'powerup',9}, {'tally',19,20}, {'stage_clear',21,22,23}, {'gameover',24,25,26}
}
local f = assert(io.open(output .. '/sound-durations.csv','w'))
f:write('sound,frames,loop\n')
for frame = 1,450 do joypad.set(1,{}); FCEU.frameadvance() end
-- Start every request from the same idle state.  Without this, a looping
-- engine/music channel left over from the previous event contaminates later
-- measurements.
local state=savestate.create()
savestate.save(state)
for _,event in ipairs(definitions) do
    savestate.load(state)
    for at = 0x300,0x3fb do memory.writebyte(at,0) end
    for i = 2,#event do memory.writebyte(0x300+event[i],1) end
    local duration, loop = 0, true
    for frame = 1,600 do
        joypad.set(1,{}); FCEU.frameadvance()
        local active = false
        for i = 2,#event do
            if memory.readbyte(0x300+event[i]) ~= 0 then active = true end
        end
        duration = frame
        if not active then loop = false; break end
    end
    f:write(string.format('%s,%d,%s\n',event[1],duration,tostring(loop)))
end
f:close()
os.exit(0)
