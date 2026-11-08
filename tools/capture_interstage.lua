FCEU.speedmode('maximum')
local out = os.getenv('BATTLECITY_TRACE_DIR') or os.getenv('BC_OUT') or '.'
for frame = 1, 800 do
    local keys = {}
    if frame >= 500 and frame < 505 then keys.start = true end
    joypad.set(1, keys)
    FCEU.frameadvance()
end
local f = assert(io.open(out .. '/stage_select-ppu.bin', 'wb'))
for i = 0, 0x3fff do f:write(string.char(ppu.readbyte(i))) end
f:close()
local r = assert(io.open(out .. '/stage_select-ram.bin', 'wb'))
for i = 0, 0x7ff do r:write(string.char(memory.readbyte(i))) end
r:close()
local g = assert(io.open(out .. '/stage_select.gd', 'wb'))
g:write(gui.gdscreenshot())
g:close()
os.exit(0)
