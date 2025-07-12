-- Test the cwc_tag property

local enum = require("cuteful.enum")

local cwc = cwc

local function test()
    local s = cwc.screen.focused()
    local tag = s:get_tag(3)

    tag:strategy_idx(1)
    tag:view_only()

    assert(tag.index == 3)

    assert(tag.selected)
    tag.selected = not tag.selected
    assert(tag.selected == false)
    tag:toggle()
    assert(tag.selected)

    assert(tag.gap >= 0)
    tag.gap = 3
    assert(tag.gap == 3)

    assert(tag.layout_mode >= 0 and tag.layout_mode < enum.layout_mode.LENGTH)
    tag.layout_mode = enum.layout_mode.BSP
    assert(tag.layout_mode == enum.layout_mode.BSP)

    assert(tag.mwfact == 0.5)
    tag.mwfact = 99999
    assert(tag.mwfact == 0.9)

    assert(tostring(tag.screen):match("cwc_screen"))

    assert(tag.master_count == 1)
    tag.master_count = 2
    assert(tag.master_count == 2)

    assert(tag.column_count == 1)
    tag.column_count = 3
    assert(tag.column_count == 3)

    print("cwc_tag test \27[1;32mPASSED\27[0m")
end

return test
