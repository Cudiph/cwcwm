-- Test the cwc_client property
local bit = require("bit")
local enum = require("cuteful.enum")
local gears = require("gears")

local cwc = cwc

local signal_list = {
    "client::new",
    "client::destroy",
    "client::map",
    "client::unmap",
    "client::focus",
    "client::unfocus",
    "client::swap",
    -- "client::mouse_enter", -- require interaction
    -- "client::mouse_leave", -- require interaction
    "client::raised",
    "client::lowered",
    "client::property::fullscreen",
    "client::property::maximized",
    "client::property::minimized",
    "client::property::floating",
    "client::property::urgent",
    "client::property::tag",
    "client::property::workspace",
    "client::prop::title",
    "client::prop::appid",
}

local triggered_list = {}

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
        print(string.format("%d of %d cwc_client signal test \27[1;31mFAILED\27[0m", count, #signal_list))
    else
        print("cwc_client signal test \27[1;32mPASSED\27[0m")
    end
end



local function static_test()
    local cls = cwc.client.get()
    assert(#cls == 20)
end

local function readonly_test(c)
    assert(type(c.mapped) == "boolean")
    assert(type(c.visible) == "boolean")
    assert(type(c.x11) == "boolean")

    -- assert(string.find(tostring(c.parent), "cwc_client"))
    assert(string.find(tostring(c.screen), "cwc_screen"))
    assert(type(c.pid) == "number")
    assert(type(c.title) == "string")
    assert(type(c.appid) == "string")
    assert(string.find(tostring(c.container), "cwc_container"))
end

local function property_test(c)
    assert(c.fullscreen == false)
    c.fullscreen = true
    assert(c.fullscreen)
    c.fullscreen = false
    assert(not c.fullscreen)

    assert(c.maximized == false)
    c.maximized = true
    assert(c.maximized)
    c.maximized = false
    assert(not c.maximized)

    assert(type(c.floating) == "boolean")
    c.floating = true
    assert(c.floating)

    assert(c.minimized == false)
    c.minimized = true
    assert(c.minimized)
    c.minimized = false
    assert(not c.minimized)

    assert(c.sticky == false)
    assert(c.ontop == false)
    assert(c.above == false)
    assert(c.below == false)
    c.ontop = true
    c.below = true
    c.above = true
    assert(c.above)
    assert(c.below == false)
    assert(c.ontop == false)

    local geom = c.geometry
    assert(type(geom) == "table")
    assert(type(geom.x) == "number")
    assert(type(geom.y) == "number")
    assert(type(geom.width) == "number")
    assert(type(geom.height) == "number")

    assert(c.tag > 0)
    assert(c.workspace > 0)
    c.workspace = 5
    assert(c.tag == bit.lshift(1, 4))
    assert(c.workspace == 5)

    assert(c.opacity >= 0 and c.opacity <= 1)
    c.opacity = 0.5
    assert(c.opacity == 0.5)

    assert(not c.allow_tearing)
    c.allow_tearing = true
    assert(c.allow_tearing)

    assert(type(c.border_width) == "number")
    c.border_width = 2
    assert(c.border_width == 2)

    assert(type(c.border_rotation) == "number")
    c.border_rotation = 90
    assert(c.border_rotation == 90)
end

local function method_test(c)
    local cls = cwc.client.get()
    local rand = cls[math.random(#cls)]
    c:set_border_color(gears.color("#ffffff"))
    c:raise()
    c:lower()
    c:focus()
    c:jump_to(true)
    c:jump_to()
    c:swap(rand)
    c:center()
    c:move_to_tag(c.workspace)
    c:toggle_split()
    c:toggle_tag(c.workspace + 1)
    c:close()
    c:move_to_screen(cwc.screen.focused())
    rand:kill()
end

local function test()
    static_test()

    local c = cwc.client.focused()
    readonly_test(c)
    property_test(c)
    method_test(c)

    print("cwc_client test \27[1;32mPASSED\27[0m")
end


return {
    api = test,
    signal = signal_check,
}
