local cful = require("cuteful")

local scr_test = require("luapi.screen")
local client_test = require("luapi.client")
local capi_signal = require("capi.signal")
local lua_signal = require("luapi.signal")
local container_test = require("luapi.container")
local tag_test = require("luapi.tag")

local cwc = cwc

local mod = cful.enum.modifier


local MODKEY = mod.LOGO
if cwc.is_nested() then
    MODKEY = mod.ALT
end

-- start the test by pressing f12
cwc.kbd.bind({}, "F12", function()
    print("--------------------------------- TEST START ------------------------------------")
    client_test()
    scr_test()
    capi_signal()
    lua_signal()
    tag_test()

    cwc.screen.focused():get_tag(2):view_only()
    container_test()
end)

cwc.kbd.bind({ MODKEY, mod.CTRL }, "r", cwc.reload, { description = "reload configuration" })

local tagidx = 1
cwc.connect_signal("client::map", function(c)
    c:focus()
    c:move_to_tag(tagidx)
    tagidx = tagidx + 1
end)

-- make a client available on all tags in case the test need client
for _ = 1, 9 do
    cwc.spawn_with_shell("kitty")
end

local kbd = cwc.kbd
for i_tag = 1, 9 do
    kbd.bind(MODKEY, i_tag, function()
        local s = cwc.screen.focused()
        s:view_only(i_tag)
    end, { description = "view tag #" .. i_tag, group = "tag" })

    kbd.bind({ MODKEY, mod.CTRL }, i_tag, function()
        local s = cwc.screen.focused()
        if not s then return end

        s:toggle_tag(i_tag)
    end, { description = "toggle tag #" .. i_tag, group = "tag" })

    kbd.bind({ MODKEY, mod.SHIFT }, i_tag, function()
        local c = cwc.client.focused()
        if not c then return end

        c:move_to_tag(i_tag)
    end, { description = "move focused client to tag #" .. i_tag, group = "tag" })

    kbd.bind({ MODKEY, mod.SHIFT, mod.CTRL }, i_tag, function()
        local c = cwc.client.focused()
        if not c then return end

        c:toggle_tag(i_tag)
    end, { description = "toggle focused client on tag #" .. i_tag, group = "tag" })
end
