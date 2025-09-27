local gears = require("gears")
local config = require("config")

local cwc = cwc

local focus = config["border_color_focus"]
local normal = config["border_color_normal"]

cwc.connect_signal("client::focus", function(c)
    c.border_color = focus
end)

cwc.connect_signal("client::unfocus", function(c)
    c.border_color = normal
end)

cwc.connect_signal("client::swap", function(...)
    local clients = { ... }
    local focused = cwc.client.focused()
    for _, c in pairs(clients) do
        if focused == c then
            c.border_color = focus
        else
            c.border_color = normal
        end
    end
end)

cwc.connect_signal("container::swap", function(...)
    local conts = { ... }
    local focused = cwc.client.focused()
    for _, cont in ipairs(conts) do
        cont.front.border_color = normal
        for _, c in ipairs(cont.clients) do
            if c == focused then
                c.border_color = focus
                break
            end
        end
    end
end)
