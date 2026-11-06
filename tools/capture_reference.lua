-- Capture the title and intact stage-one PPU/RAM snapshots used to generate
-- src/stage1.h.  Set BATTLECITY_TRACE_DIR to choose the output directory.
FCEU.speedmode('maximum')
local root = debug.getinfo(1, 'S').source:sub(2):match('^(.*)/tools/[^/]+$') or '.'
local output = os.getenv('BATTLECITY_TRACE_DIR') or (root .. '/build/rom-trace')
os.execute('mkdir -p ' .. output)
local function dump(name, count, read, base)
    local f = assert(io.open(output .. '/' .. name, 'wb'))
    for i = 0, count - 1 do f:write(string.char(read(base + i))) end
    f:close()
end
for frame = 1, 1350 do
    joypad.set(1, {start = (frame >= 500 and frame < 505) or (frame >= 1000 and frame < 1005)})
    FCEU.frameadvance()
    if frame == 450 or frame == 1300 then
        local prefix = frame == 450 and 'title' or 'stage1'
        dump(prefix .. '-ppu.bin', 0x4000, ppu.readbyte, 0)
        dump(prefix .. '-ram.bin', 0x800, memory.readbyte, 0)
        local f = assert(io.open(output .. '/' .. prefix .. '.gd', 'wb'))
        f:write(gui.gdscreenshot()); f:close()
    end
end
local f = assert(io.open(output .. '/capture.done', 'w'))
f:write('1350 frames captured\n'); f:close()
os.exit(0)
