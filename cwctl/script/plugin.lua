local cwc = cwc

local function plugin_list()
    local plugins = cwc.plugin.get()
    local out = ""

    for p_idx, plug in pairs(plugins) do
        if out then out = out .. "\n" end
        local template =
            "[%d] %s (%s):\n" ..
            "\tName: %s\n" ..
            "\tDescription: %s\n" ..
            "\tVersion: %s\n" ..
            "\tAuthor: %s\n" ..
            "\tLicense: %s\n" ..
            "\tFilename: %s\n" ..
            ""

        out = out .. string.format(template,
            p_idx, plug.name, plug,
            plug.name,
            plug.description,
            plug.version,
            plug.author,
            plug.license,
            plug.filename
        )
    end

    return out
end

return plugin_list()
