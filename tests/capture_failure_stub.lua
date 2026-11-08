-- Minimal FCEUX stubs for exercising capture-script failure paths with Lua.
-- Usage: lua tests/capture_failure_stub.lua tools/capture_tally.lua
local target = assert(arg[1], 'capture script path')

FCEU = {
    speedmode = function() end,
    frameadvance = function() end,
}
joypad = { set = function() end }
memory = {
    readbyte = function() return 0 end,
    writebyte = function() end,
    registerwrite = function() end,
}
ppu = { readbyte = function() return 0 end }
gui = { gdscreenshot = function() return '' end }

dofile(target)
