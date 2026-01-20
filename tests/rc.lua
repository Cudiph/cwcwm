local cful = require("cuteful")
local config = require("config")

local screen_test = require("luapi.screen")
local client_test = require("luapi.client")
local capi_signal = require("capi.signal")
local lua_signal = require("luapi.signal")
local container_test = require("luapi.container")
local tag_test = require("luapi.tag")
local layershell_test = require("luapi.layer_shell")
local kbinding_test = require("luapi.kbinding")
local plugin_test = require("luapi.plugin")
local pointer_test = require("luapi.pointer")
local kbd_test = require("luapi.kbd")

local cwc = cwc

config.init({
    xkb_variant = "colemak",
    xkb_layout  = "us,de,fr",
    xkb_options = "grp:alt_shift_toggle,grp:caps_select",
})

local mod = cful.enum.modifier

local MODKEY = mod.LOGO
if cwc.is_nested() then
    MODKEY = mod.ALT
end
print(MODKEY == mod.ALT)

-- 2 client at each tag
local tagidx = 1
local counter = 0
cwc.connect_signal("client::map", function(c)
    c:focus()
    c:move_to_tag(tagidx)
    counter = counter + 1
    if counter == 2 then
        counter = 0
        tagidx = tagidx + 1
    end
end)

-- make a client available on all tags in case the test need client
for _ = 1, 20 do
    cwc.spawn_with_shell("kitty")
end

-- spawn waybar for layer shell testing
cwc.spawn({ "waybar", "-c", "/etc/xdg/waybar/config.jsonc" })

local kbd = cwc.kbd
for i = 1, 9 do
    local i_str = tostring(i)
    kbd.bind(MODKEY, i_str, function()
        local t = cwc.screen.focused():get_tag(i)
        t:view_only()
    end, { description = "view tag #" .. i_str, group = "tag" })

    kbd.bind({ MODKEY, mod.CTRL }, i_str, function()
        local t = cwc.screen.focused():get_tag(i)
        t:toggle()
    end, { description = "toggle tag #" .. i_str, group = "tag" })

    kbd.bind({ MODKEY, mod.SHIFT }, i_str, function()
        local c = cwc.client.focused()
        if not c then return end

        c:move_to_tag(i_str)
    end, { description = "move focused client to tag #" .. i_str, group = "tag" })

    kbd.bind({ MODKEY, mod.SHIFT, mod.CTRL }, i_str, function()
        local c = cwc.client.focused()
        if not c then return end

        c:toggle_tag(i_str)
    end, { description = "toggle focused client on tag #" .. i_str, group = "tag" })
end

-- start API test by pressing F12
cwc.kbd.bind({}, "F12", function()
    print("\n--------------------------------- API TEST START ------------------------------------")
    client_test.api()
    screen_test.api()
    layershell_test.api()
    capi_signal()
    lua_signal()
    tag_test.api()
    kbinding_test()
    plugin_test.api()
    pointer_test.api()
    kbd_test.api()

    cwc.screen.focused():get_tag(2):view_only()
    container_test.api()
    print("--------------------------------- API TEST END ------------------------------------")
    io.flush()
end)

-- signal test by pressing F1 (must execute after API test)
cwc.kbd.bind({}, "F1", function()
    print(
        "\n--------------------------------- SIGNAL TEST START ------------------------------------")
    client_test.signal()
    screen_test.signal()
    container_test.signal()
    layershell_test.signal()
    tag_test.signal()
    plugin_test.signal()
    pointer_test.signal()
    kbd_test.signal()
    print("--------------------------------- SIGNAL TEST END ------------------------------------")
    io.flush()
end)

cwc.kbd.bind({ MODKEY, mod.CTRL }, "r", cwc.reload, { description = "reload configuration" })

-- automatic start
cwc.timer.new(3, function()
    cful.kbd.click(cful.kbd.event_code.KEY_F12)
    cwc.timer.new(3, function()
        cful.kbd.click(cful.kbd.event_code.KEY_F1)
    end, { one_shot = true })
end, { one_shot = true })
