-- Deterministic ROM input/OAM trace, independent of the static reference captures.
-- Run from the repository root. BATTLECITY_TRACE_DIR overrides the output directory.
FCEU.speedmode('maximum')
local root = debug.getinfo(1, 'S').source:sub(2):match('^(.*)/tools/[^/]+$') or '.'
local output = os.getenv('BATTLECITY_TRACE_DIR') or (root .. '/build/rom-trace')
local trace = assert(io.open(output .. '/oam.csv', 'w'))
trace:write('frame,input,slot,y,tile,attributes,x\n')
local audio = assert(io.open(output .. '/apu.csv', 'w'))
audio:write('frame,address,value,pc\n')
local current_frame = 0
local events = assert(io.open(output .. '/sound-events.csv', 'w'))
events:write('frame,event,value,pc\n')
for address = 0x300, 0x31b do
    memory.registerwrite(address, function(at, size, value)
        if value == 0 or value == 1 then
            events:write(string.format('%d,%d,%d,%d\n', current_frame, at - 0x300,
                value, memory.getregister('pc')))
        end
    end)
end
for address = 0x4000, 0x4017 do
    if address <= 0x4013 or address == 0x4015 or address == 0x4017 then
        memory.registerwrite(address, function(at, size, value)
            audio:write(string.format('%d,%d,%d,%d\n',current_frame,at,value,memory.getregister('pc')))
        end)
    end
end
local function dump(name, count, read, base)
    local f = assert(io.open(output .. '/' .. name, 'wb'))
    for i = 0, count - 1 do f:write(string.char(read(base + i))) end
    f:close()
end
for frame = 1, 1800 do
    current_frame = frame
    local keys, label = {}, 'idle'
    if (frame >= 500 and frame < 505) or (frame >= 1000 and frame < 1005) then
        keys.start = true
    end
    if frame >= 1301 and frame <= 1340 then keys.right = true; label = 'right'
    elseif frame >= 1341 and frame <= 1380 then keys.up = true; label = 'up'
    elseif frame >= 1381 and frame <= 1420 then keys.left = true; label = 'left'
    elseif frame >= 1421 and frame <= 1460 then keys.down = true; label = 'down'
    elseif frame >= 1461 and frame <= 1600 then keys.up = true; keys.A = true; label = 'fire-up'
    end
    joypad.set(1, keys)
    FCEU.frameadvance()
    if frame >= 1000 then
        for slot = 0, 63 do
            local at = 0x200 + slot * 4
            local y = memory.readbyte(at)
            if y < 239 then
                trace:write(string.format('%d,%s,%d,%d,%d,%d,%d\n', frame, label, slot,
                    y, memory.readbyte(at + 1), memory.readbyte(at + 2), memory.readbyte(at + 3)))
            end
        end
    end
    if frame == 1100 or frame == 1200 or frame == 1300 or frame == 1330 or frame == 1370
        or frame == 1410 or frame == 1450 or frame == 1500 or frame == 1550 or frame == 1600 then
        local name = tostring(frame)
        dump(name .. '-ram.bin', 0x800, memory.readbyte, 0)
        dump(name .. '-ppu.bin', 0x4000, ppu.readbyte, 0)
        local f = assert(io.open(output .. '/' .. name .. '.gd', 'wb'))
        f:write(gui.gdscreenshot()); f:close()
    end
end
trace:close()
audio:close()
events:close()
local done = assert(io.open(output .. '/capture.done', 'w'))
done:write('1800 frames captured\n'); done:close()
os.exit(0)
