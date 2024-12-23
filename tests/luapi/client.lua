-- Test the cwc_client property
local bit = require("bit")
local enum = require("cuteful.enum")

local cwc = cwc

local function static_test()
    local cls = cwc.client.get()
    assert(#cls == 9)
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

    print(c.title)
    print(c.pid)
    print(c.appid)
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
end

local function method_test(c)
    c:raise()
    c:lower()
    c:focus()
    c:swap(c)
    c:center()
    c:move_to_tag(c.workspace)
    assert(c:get_nearest(enum.direction.LEFT) == nil)
    c:toggle_split()
    c:toggle_tag(c.workspace + 1)
end

local function test()
    static_test()

    local c = cwc.client.focused()
    readonly_test(c)
    property_test(c)
    method_test(c)

    print("cwc_client test PASSED")
end


return test
