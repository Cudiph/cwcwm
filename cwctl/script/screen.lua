local bit = require("bit")
local gears = require("gears")

local cwc = cwc

local function scr_list()
    local scrs = cwc.screen.get()
    local out = ""

    for s_idx, s in pairs(scrs) do
        if out then out = out .. "\n" end
        local active_tags = ""

        for i = 1, s.max_general_workspace do
            if bit.band(s.active_tag, bit.lshift(1, i - 1)) ~= 0 then
                if #active_tags ~= 0 then active_tags = active_tags .. ", " end
                active_tags = active_tags .. i
            end
        end

        local template =
            "[%d] %s (%s):\n" ..
            "\tDescription: %s\n" ..
            "\tEnabled: %s\n" ..
            "\tDPMS: %s\n" ..
            "\tMake: %s\n" ..
            "\tModel: %s\n" ..
            "\tSerial: %s\n" ..
            "\tMode: %sx%s@%s\n" ..
            "\tPosition: %s,%s\n" ..
            "\tPhysical size: %dx%d mm\n" ..
            "\tScale: %f\n" ..
            "\tScreen geometry:\n" ..
            "\t\tx: %d\n" ..
            "\t\ty: %d\n" ..
            "\t\tw: %d\n" ..
            "\t\th: %d\n" ..
            "\tScreen work area:\n" ..
            "\t\tx: %d\n" ..
            "\t\ty: %d\n" ..
            "\t\tw: %d\n" ..
            "\t\th: %d\n" ..
            "\tNon desktop: %s\n" ..
            "\tRestored: %s\n" ..
            "\tSelected tag: %d\n" ..
            "\tActive tags: %s\n" ..
            "\tTearing allowed: %s\n" ..
            "\tHas focus: %s\n" ..
            ""

        out = out .. string.format(template,
            s_idx, s.name, s,
            s.description,
            s.enabled,
            s.dpms and "on" or "off",
            s.make,
            s.model,
            s.serial,
            s.width, s.height, s.refresh / 1000,
            s.geometry.x, s.geometry.y,
            s.phys_width, s.phys_height,
            s.scale,
            s.geometry.x, s.geometry.y, s.geometry.width, s.geometry.height,
            s.workarea.x, s.workarea.y, s.workarea.width, s.workarea.height,
            s.non_desktop,
            s.restored,
            s.selected_tag.index,
            active_tags,
            s.allow_tearing,
            cwc.screen.focused() == s
        )
    end

    return out
end

local function scr_set(filter, key, value)
    local scrs = cwc.screen.get()
    local focused = cwc.screen.focused()

    if scrs[1][key] == nil then
        return "error: screen `" .. key .. "` property is read only/doesn't exist"
    end

    for _, s in pairs(scrs) do
        if value == "toggle" then value = not s[key] end

        if filter == "*" then
            s[key] = value
        elseif filter == "focused" and s == focused then
            s[key] = value
            break
        elseif filter == s.name then
            s[key] = value
            break
        end
    end

    return "OK"
end

local function scr_get(filter, key)
    local scrs = cwc.screen.get()
    local focused = cwc.screen.focused()

    if scrs[1][key] == nil then
        return "error: screen `" .. key .. "` property is empty or doesn't exist"
    end

    local out = ""
    for i, s in pairs(scrs) do
        local formatted = string.format('\n[%d] "%s".%s = %s \n', i, s.name, key,
            gears.debug.dump_return(s[key]))

        if filter == "*" then
            out = out .. formatted
        elseif filter == "focused" and s == focused then
            out = out .. formatted
            break
        elseif filter == s.name then
            out = out .. formatted
            break
        end
    end

    return out
end
