local bit = require("bit")

local cwc = cwc

local function c_list()
    local cls = cwc.client.get()
    local out = ""

    for c_idx, c in pairs(cls) do
        if out then out = out .. "\n" end
        local tag = ""

        for i = 1, c.screen.max_general_workspace do
            if bit.band(c.tag, bit.lshift(1, i - 1)) ~= 0 then
                if #tag ~= 0 then tag = tag .. ", " end
                tag = tag .. i
            end
        end


        local template =
            "[%d] %s (%s):\n" ..
            "\tApp ID: %s\n" ..
            "\tPID: %s\n" ..
            "\tXwayland: %s\n" ..
            "\tUnmanaged: %s\n" ..
            "\tVisible: %s\n" ..
            "\tMapped: %s\n" ..
            "\tFullscreen: %s\n" ..
            "\tMaximized: %s\n" ..
            "\tFloating: %s\n" ..
            "\tMinimized: %s\n" ..
            "\tSticky: %s\n" ..
            "\tOntop: %s\n" ..
            "\tAbove: %s\n" ..
            "\tBelow: %s\n" ..
            "\tTearing Allowed: %s\n" ..
            "\tUrgent: %s\n" ..
            "\tTag: %s\n" ..
            "\tOpacity: %f\n" ..
            "\tBorder: %s, %d px, %d deg\n" ..
            "\tParent: %s\n" ..
            "\tScreen: %s\n" ..
            "\tContainer: %s\n" ..
            "\tGeometry:\n" ..
            "\t\tx: %d\n" ..
            "\t\ty: %d\n" ..
            "\t\tw: %d\n" ..
            "\t\th: %d\n" ..
            "\n"

        out = out .. string.format(template,
            c_idx, c.title, c,
            c.appid,
            c.pid,
            c.x11,
            c.unmanaged,
            c.visible,
            c.mapped,
            c.fullscreen,
            c.maximized,
            c.floating,
            c.minimized,
            c.sticky,
            c.ontop,
            c.above,
            c.below,
            c.allow_tearing,
            c.urgent,
            tag,
            c.opacity,
            c.border_enabled, c.border_width, c.border_rotation,
            c.parent,
            c.screen,
            c.container,
            c.geometry.x, c.geometry.y, c.geometry.width, c.geometry.height
        )
    end

    return out
end

return c_list()
