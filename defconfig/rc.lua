-- cwc default config

-- If LuaRocks is installed, make sure that packages installed through it are
-- found (e.g. lgi). If LuaRocks is not installed, do nothing.
pcall(require, "luarocks.loader")

local gears = require("gears")
local enum = require("cuteful.enum")
local tag = require("cuteful.tag")
local impl = require("impl")
local config = require("config")

-- make it local so the `undefined global` lsp error stop yapping on every cwc access
local cwc = cwc

-- config.init should go first before anything else
config.init(require("conf"))

-- execute oneshot.lua once, cwc.is_startup() mark that the configuration is loaded for the first time
if cwc.is_startup() then
    gears.protected_call(require, "oneshot")
end

-- execute keybind script
gears.protected_call(require, "keybind")

-- use core implementation
impl.use_core()

-- input device config
cwc.connect_signal("input::new", function(dev)
    dev.sensitivity   = -0.75
    dev.accel_profile = enum.libinput.ACCEL_PROFILE_FLAT

    if dev.name:lower():match("touchpad") then
        dev.sensitivity    = 0.7
        dev.natural_scroll = true
        dev.tap            = true
        dev.tap_drag       = true
        dev.dwt            = true
    end
end)

------------------------------- SCREEN SETUP ------------------------------------
cwc.connect_signal("screen::new", function(screen)
    -- screen settings
    if screen.name == "DP-1" then
        screen:set_position(0, 0)

        screen:set_mode(1920, 1080, 60) -- width, height, refresh rate
        screen:set_adaptive_sync(true)
        screen:set_scale(1.2)
        screen:set_transform(enum.output_transform.TRANSFORM_NORMAL)

        -- by default the screen is not allowed to tear
        screen.allow_tearing = true
    end

    if screen.name == "DP-2" then
        screen:set_position(1920, 0)
    end

    -- don't apply if restored since it will reset whats manually changed
    if screen.restored then return end

    -- set all "general" tags to master/stack mode by default
    for i = 1, 9 do
        tag.layout_mode(i, enum.layout_mode.MASTER, screen)
    end

    -- set workspace 2, 8, and 9 to floating mode
    tag.layout_mode(2, enum.layout_mode.FLOATING, screen)
    tag.layout_mode(8, enum.layout_mode.FLOATING, screen)
    tag.layout_mode(9, enum.layout_mode.FLOATING, screen)

    -- set workspace 4, 5, 6 to bsp mode
    tag.layout_mode(4, enum.layout_mode.BSP, screen)
    tag.layout_mode(5, enum.layout_mode.BSP, screen)
    tag.layout_mode(6, enum.layout_mode.BSP, screen)
end)

-- cwc.connect_signal("screen::destroy", function(screen)
--     --- here screen.clients is equivalent as screen:get_clients()
--     local cmd = string.format(
--         'notify-send "Screen removed" "Screen %s [%s] with %s clients attached"', screen.name,
--         screen.description or "-", #screen.clients)
--     cwc.spawn_with_shell(cmd)
-- end)

------------------------ CLIENT BEHAVIOR -----------------------------
cwc.connect_signal("client::map", function(client)
    -- unmanaged client is a popup/tooltip client in xwayland so lets skip it.
    if client.unmanaged then return end

    -- center the client from the screen workarea if its floating or in floating layout.
    if client.floating then client:center() end

    -- don't pass focus when the focused client is fullscreen but allow if the parent is the focused
    -- one. Useful when gaming where an app may restart itself and steal focus.
    local focused = cwc.client.focused()
    if focused and focused.fullscreen and client.parent ~= focused then
        client:lower()
        return
    end

    client:raise()
    client:focus()

    -- the declarative rules isn't implemented yet so here is an example to do ruling.
    -- It'll move any firefox app to the workspace 2 and maximize it also we moving to tag 2.
    if client.appid == "firefox" then
        client:move_to_tag(2)
        client.screen.active_workspace = 2
    end

    if client.appid:match("pcmanfm") then
        client.floating = true
        client:center()
    end
end)

cwc.connect_signal("client::unmap", function(client)
    -- exit when the unmapped client is not the focused client.
    if client ~= cwc.client.focused() then return end
    -- and for unmanaged client
    if client.unmanaged then return end

    -- if the client container has more than one client then we focus just below the unmapped
    -- client
    local cont_stack = client.container.client_stack
    if #cont_stack > 1 then
        cont_stack[2]:focus()
    else
        -- get the focus stack (first item is the newest) and we shift focus to the second newest
        -- since first one is about to be unmapped from the screen.
        local latest_focus_after = client.screen:get_focus_stack(true)[2]
        if latest_focus_after then latest_focus_after:focus() end
    end
end)

cwc.connect_signal("client::focus", function(client)
    -- by default when a client got focus it's not raised so we raise it.
    -- should've been hardcoded to the compositor since that's the intuitive behavior
    -- but it's nice to have option I guess.
    client:raise()
end)

-- sloppic focus only in tiled client
cwc.connect_signal("client::mouse_enter", function(c)
    local focused = cwc.client.focused()
    if focused and focused.floating then return end

    c:focus()
end)

cwc.connect_signal("container::insert", function(cont, client)
    -- reset mark after first insertion in case forgot to toggle off mark
    cwc.container.reset_mark()

    -- focus to the newly inserted client
    client:focus()
end)

cwc.connect_signal("screen::mouse_enter", function(s)
    s:focus()
end)
