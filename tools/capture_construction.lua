-- Capture the title's CONSTRUCTION entry, the empty editor, and a deterministic
-- edit sequence.  Select twice moves the title cursor to CONSTRUCTION, Start
-- enters the editor, and the later presses exercise cursor movement, the A/B
-- block cycle, and painting while moving.
FCEU.speedmode('maximum')
local root = debug.getinfo(1, 'S').source:sub(2):match('^(.*)/tools/[^/]+$') or '.'
local output = os.getenv('BATTLECITY_TRACE_DIR') or (root .. '/build/rom-trace')
os.execute('mkdir -p ' .. output)

-- {first frame, last frame, button} windows.  Each is five frames long, so
-- the ROM sees one press edge and no keyboard auto-repeat.
local schedule = {
    {610, 614, 'right'}, {620, 624, 'right'},
    {630, 634, 'down'},  {640, 644, 'down'},
    {650, 654, 'A'}, {660, 664, 'A'}, {670, 674, 'A'},
    {680, 684, 'A'}, {690, 694, 'A'}, {700, 704, 'A'},
    {710, 714, 'right'}, {720, 724, 'right'}, {720, 724, 'A'},
    {730, 734, 'down'},  {730, 734, 'A'},
    {740, 744, 'A'}, {750, 754, 'A'}, {760, 764, 'A'},
    {770, 774, 'A'}, {780, 784, 'A'}, {790, 794, 'A'},
    {800, 804, 'right'}, {810, 814, 'right'}, {810, 814, 'A'},
    {820, 824, 'down'}, {830, 834, 'down'},
    {840, 844, 'A'}, {850, 854, 'A'},
    {860, 864, 'right'}, {870, 874, 'A'}, {880, 884, 'A'},
    {890, 894, 'right'}, {900, 904, 'B'}, {910, 914, 'B'},
    {920, 924, 'B'},
    {930, 934, 'down'}, {940, 944, 'A'}, {950, 954, 'A'},
    {960, 964, 'A'},
}

local function edit_keys(frame)
    local keys = {}
    for _, press in ipairs(schedule) do
        if frame >= press[1] and frame <= press[2] then
            keys[press[3]] = true
        end
    end
    return keys
end

local function save(prefix, want_ppu, want_ram, want_gd)
    if want_ppu then
        local p = assert(io.open(output .. '/' .. prefix .. '-ppu.bin', 'wb'))
        for i = 0, 0x3fff do p:write(string.char(ppu.readbyte(i))) end
        p:close()
    end
    if want_ram then
        local r = assert(io.open(output .. '/' .. prefix .. '-ram.bin', 'wb'))
        for i = 0, 0x7ff do r:write(string.char(memory.readbyte(i))) end
        r:close()
    end
    if want_gd then
        local g = assert(io.open(output .. '/' .. prefix .. '.gd', 'wb'))
        g:write(gui.gdscreenshot()); g:close()
    end
end

for frame = 1, 1000 do
    local keys = {}
    if frame >= 400 and frame <= 404 then keys.select = true end
    if frame >= 430 and frame <= 434 then keys.select = true end
    if frame >= 500 and frame <= 504 then keys.start = true end
    if frame >= 610 then keys = edit_keys(frame) end
    joypad.set(1, keys)
    FCEU.frameadvance()
    if frame == 490 then save('titlec', true, true, true) end
    if frame == 540 then save('construction-blink', true, true, true) end
    if frame == 600 then save('construction', true, true, true) end
    if frame == 980 then save('construction-edit', true, true, true) end
end
os.exit(0)
