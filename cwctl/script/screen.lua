local bit = require("bit")

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
            "\tMake: %s\n" ..
            "\tModel: %s\n" ..
            "\tSerial: %s\n" ..
            "\tMode: %sx%s@%s\n" ..
            "\tPosition: %s,%s\n" ..
            "\tPhysical size: %dx%d mm\n" ..
            "\tScale: %f\n" ..
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
            s.make,
            s.model,
            s.serial,
            s.width, s.height, s.refresh / 1000,
            s.geometry.x, s.geometry.y,
            s.phys_width, s.phys_height,
            s.scale,
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

return scr_list()
