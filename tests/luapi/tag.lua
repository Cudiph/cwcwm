-- Test the cwc_tag property

local enum = require("cuteful.enum")

local cwc = cwc

local signal_list = {
}

local triggered_list = {
    "tag::prop::label",
    "tag::prop::hidden",
}

for _, signame in pairs(signal_list) do
    cwc.connect_signal(signame, function()
        triggered_list[signame] = true
    end)
end

local function signal_check()
    local count = 0
    for _, signame in pairs(signal_list) do
        if not triggered_list[signame] then
            local template = string.format("signal %s is not triggered", signame)
            print(template)
            count = count + 1
        end
    end

    if count > 0 then
        print(string.format("%d of %d cwc_tag signal test \27[1;31mFAILED\27[0m", count, #signal_list))
    else
        print("cwc_tag signal test \27[1;32mPASSED\27[0m")
    end
end

local function readonly_test(tag)
    assert(tag.index == 3)
    assert(tostring(tag.screen):match("cwc_screen"))
end

local function property_test(tag)
    assert(tag.selected)
    tag.selected = not tag.selected
    assert(tag.selected == false)
    tag:toggle()
    assert(tag.selected)

    assert(tag.label == "3")
    tag.label = "W3"
    assert(tag.label == "W3")

    assert(not tag.hidden)
    tag.hidden = true
    assert(tag.hidden)

    assert(tag.gap >= 0)
    tag.gap = 3
    assert(tag.gap == 3)

    assert(tag.layout_mode >= 0 and tag.layout_mode < enum.layout_mode.LENGTH)
    tag.layout_mode = enum.layout_mode.BSP
    assert(tag.layout_mode == enum.layout_mode.BSP)

    assert(tag.mwfact == 0.5)
    tag.mwfact = 99999
    assert(tag.mwfact == 0.9)

    assert(tag.master_count == 1)
    tag.master_count = 2
    assert(tag.master_count == 2)

    assert(tag.column_count == 1)
    tag.column_count = 3
    assert(tag.column_count == 3)
end

local function method_test(tag)
    tag:strategy_idx(1)
    tag:view_only()
end

local function test()
    local s = cwc.screen.focused()
    local tag = s:get_tag(3)

    method_test(tag)
    readonly_test(tag)
    property_test(tag)

    print("cwc_tag test \27[1;32mPASSED\27[0m")
end

return {
    api = test,
    signal = signal_check,
}
