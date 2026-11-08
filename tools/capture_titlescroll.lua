-- Capture the two title-scroll reference frames.  Clearing all lives makes the
-- ROM show the brick GAME OVER screen; the title then scrolls upward at one
-- pixel per frame.  Offsets 163 and 283 from the first GAME OVER frame match
-- the checked-in title_anim=51/171 references.
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
local over = nil
local saved = {}
for frame = 1, 2400 do
    local keys = {}
    if (frame >= 500 and frame < 505) or (frame >= 1000 and frame < 1005) then keys.start = true end
    joypad.set(1, keys)
    if frame >= 1100 and frame < 1110 then memory.writebyte(0x51, 0) end
    FCEU.frameadvance()
    if not over and frame > 1100 and gameover_visible() then over = frame end
    if over and (frame == over + 163 or frame == over + 283) then
        local name = frame == over + 163 and 'titlescroll51' or 'titlescroll171'
        local f = assert(io.open(output .. '/' .. name .. '.gd', 'wb'))
        f:write(gui.gdscreenshot())
        f:close()
        saved[name] = true
    end
    if over and frame == over + 283 then break end
end
local missing = {}
for _, name in ipairs({'titlescroll51', 'titlescroll171'}) do
    if not saved[name] then missing[#missing + 1] = name end
end
if #missing > 0 then
    io.stderr:write('capture_titlescroll: missing frames: ' ..
        table.concat(missing, ',') .. '\n')
    os.exit(1)
end
os.exit(0)
