-- Test the cwc_kbindmap and cwc_kbind property

local enum = require("cuteful.enum")

local cwc = cwc
local kbd = cwc.kbd

local function test()
    local kmap = kbd.create_bindmap()
    local kmap2 = kbd.create_bindmap()

    assert(kmap.active)
    kmap.active = false
    assert(not kmap.active)
    kmap.active = true
    assert(kmap.active)
    kmap2:active_only()
    assert(not kmap.active)

    kmap:bind({ enum.modifier.LOGO }, "a", function()
        print("test")
    end, { description = "b", group = "c", exclusive = true, repeated = true })

    assert(#kmap.member)
    print("cwc_kbindmap test PASSED")

    ---- kbind -----
    local bind_a = kmap.member[1]
    assert(bind_a.description == "b")
    assert(bind_a.group == "c")

    bind_a.description = "d"
    assert(bind_a.description == "d")
    bind_a.group = "e"
    assert(bind_a.group == "e")

    assert(bind_a.repeated)
    assert(bind_a.exclusive)

    bind_a.repeated = false
    assert(not bind_a.repeated)
    bind_a.exclusive = false
    assert(not bind_a.exclusive)

    assert(#bind_a.modifier_name == 1)
    assert(#bind_a.modifier == 1)
    print(bind_a.modifier[1])
    print(bind_a.modifier_name[1])
    assert(bind_a.modifier[1] == enum.modifier.LOGO)
    assert(bind_a.modifier_name[1] == "LOGO")

    assert(bind_a.keyname == "a")
    assert(bind_a.keysym == 0x61)

    print("cwc_kbind test PASSED")
end

return test
