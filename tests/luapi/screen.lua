-- Test the cwc_screen property

local bit = require("bit")
local enum = require("cuteful.enum")

local cwc = cwc

local function ro_test(s)
    assert(s.width > 0)
    assert(s.height > 0)
    assert(type(s.refresh) == "number")
    assert(s.phys_width >= 0)
    assert(s.phys_height >= 0)
    assert(s.scale >= 0)
    assert(#s.name > 0)
    assert(type(s.description) == "string")
    -- assert(type(s.make) == "string")
    -- assert(type(s.model) == "string")
    -- assert(type(s.serial) == "string")
    assert(type(s.enabled) == "boolean")
    assert(type(s.non_desktop) == "boolean")
    assert(type(s.restored) == "boolean")

    assert(type(s.workarea) == "table")
    assert(s.workarea.x >= 0)
    assert(s.workarea.y >= 0)
    assert(s.workarea.width >= 0)
    assert(s.workarea.height >= 0)
end

local function prop_test(s)
    -- when cwc start and the output is created, it always start at view/workspace 1
    assert(s.active_workspace == 1)
    assert(s.active_tag == 1)

    s:view_only(5)
    assert(s:get_active_workspace() == 5)
    assert(s:get_active_tag() == bit.lshift(1, 5 - 1))

    s.active_workspace = 8
    assert(s.active_workspace == 8)
    assert(s.active_tag == bit.lshift(1, 8 - 1))

    s.active_tag = bit.bor(bit.lshift(1, 5 - 1), bit.lshift(1, 3 - 1))

    s:toggle_tag(4)
    assert(s.active_tag == bit.bor(bit.lshift(1, 5 - 1), bit.lshift(1, 3 - 1), bit.lshift(1, 4 - 1)))

    assert(s.max_general_workspace == 9)
    s.max_general_workspace = 3
    assert(s:get_max_general_workspace() == 3)
    s.max_general_workspace = -1
    assert(s.max_general_workspace == 1)
    s:set_max_general_workspace(10000)
    assert(s.max_general_workspace == cwc.screen.get_max_workspace())

    assert(s.useless_gaps >= 0)
    s.useless_gaps = 3
    assert(s.useless_gaps == 3)

    assert(s.layout_mode >= 0 and s.layout_mode < enum.layout_mode.LENGTH)
    s.layout_mode = enum.layout_mode.BSP
    assert(s.layout_mode == enum.layout_mode.BSP)

    assert(s.mwfact == 0.5)
    s.mwfact = 99999
    assert(s.mwfact == 0.9)

    assert(not s.allow_tearing)
    s.allow_tearing = true
    assert(s.allow_tearing)
end

local function method_test(s)
    s:strategy_idx(1)
    s:view_only(1)
    assert(#s.focus_stack == #s.containers)
    assert(#s.clients == #s:get_clients())
    assert(#s.containers == #s:get_containers())
    assert(#s.minimized == #s:get_minimized())
end

local function test()
    local s = cwc.screen.focused()

    ro_test(s)
    prop_test(s)
    method_test(s)

    print("cwc_screen test PASSED")
end

return test
