-- Test the cwc_kbindmap and cwc_kbind property

local enum = require("cuteful.enum")

local cwc = cwc
local kbd = cwc.kbd

local function kbd_test()
    kbd.rules = "yktrasdi"
    assert(kbd.rules == "yktrasdi")
    kbd.model = "Jennifer Lawrence"
    assert(kbd.model == "Jennifer Lawrence")
    kbd.xkb_layout = "us,de,fr"
    assert(kbd.get_xkb_layout() == "us,de,fr")
    kbd.set_xkb_variant("colemak")
    assert(kbd.xkb_variant == "colemak")
    kbd.xkb_options = "grp:ctrl_shift_toggle"
    assert(kbd.xkb_options == "grp:ctrl_shift_toggle")
end

local function test()
    kbd_test()

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

    kmap:clear()
    assert(#kmap.member == 0)

    kmap:destroy()
    local success = pcall(function()
        print(kmap.active)
    end)
    assert(not success)

    print("cwc_kbd && cwc_kbindmap && cwc_kbind test \27[1;32m\27[1;32mPASSED\27[0m\27[0m")
end

return test
