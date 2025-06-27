--- Generate markdown for keybind

local cwc = cwc
local map_by_group = {}

local defbinds = cwc.kbd.get_default_member()

local function insert_to_map_by_group(kbind_list)
    for _, kbind in pairs(kbind_list) do
        if not kbind.group then goto continue end
        if not map_by_group[kbind.group] then map_by_group[kbind.group] = {} end

        table.insert(map_by_group[kbind.group], kbind)
        ::continue::
    end
end

insert_to_map_by_group(defbinds)

local bindmaps = cwc.kbd.get_bindmap()
for _, bindmap in pairs(bindmaps) do
    if not bindmap.active then goto continue end
    insert_to_map_by_group(bindmap.member)
    ::continue::
end

local out = ""

for group, ls in pairs(map_by_group) do
    out = out .. string.format("### %s\n\n", group)

    local result_list = {}

    for _, bind in ipairs(ls) do
        local mods = ""
        for i = #bind.modifier_name, 1, -1 do
            local modname = bind.modifier_name[i]
            if modname == "LOGO" then modname = "Super" end
            if #mods ~= 0 then mods = mods .. " + " end
            mods = mods .. modname
        end

        if #mods ~= 0 then mods = mods .. " + " end
        mods = mods .. bind.keyname

        table.insert(result_list, string.format("- `%s` %s\n", mods, bind.description))
    end

    table.sort(result_list, function(a, b)
        return a < b
    end)

    for _, res in pairs(result_list) do
        out = out .. res
    end

    out = out .. "\n"
end

return out
