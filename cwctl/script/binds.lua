local cwc = cwc
local kbd = cwc.kbd

local function binds_list()
    local binds = {}
    local out = ""

    for _, bind in pairs(kbd.get_default_member()) do
        table.insert(binds, bind)
    end

    for _, kbind in pairs(kbd.get_bindmap()) do
        if kbind.active then
            for _, bind in pairs(kbind.member) do
                table.insert(binds, bind)
            end
        end
    end

    for idx, bind in pairs(binds) do
        if out then out = out .. "\n" end
        local mod_and_keyname = ""

        for _, modname in pairs(bind.modifier_name) do
            if #mod_and_keyname > 0 then
                mod_and_keyname = mod_and_keyname .. " + "
            end

            mod_and_keyname = mod_and_keyname .. modname
        end

        if #mod_and_keyname > 0 then
            mod_and_keyname = mod_and_keyname .. " + " .. bind.keyname
        else
            mod_and_keyname = bind.keyname
        end


        local modnum_list = ''
        for _, mod_enum in pairs(bind.modifier) do
            if #modnum_list > 0 then modnum_list = modnum_list .. ", " end
            modnum_list = modnum_list .. mod_enum
        end

        local template =
            "[%d] %s (%s):\n" ..
            "\tGroup: %s\n" ..
            "\tDescription: %s\n" ..
            "\tExclusive: %s\n" ..
            "\tRepeat: %s\n" ..
            "\tModifier: %s\n" ..
            "\tKeysym: 0x%x\n" ..
            ""

        out = out .. string.format(template,
            idx, mod_and_keyname, bind,
            bind.group or "-",
            bind.description or "-",
            bind.exclusive,
            bind.repeated,
            #modnum_list > 0 and modnum_list or '-',
            bind.keysym
        )
    end

    return out
end

return binds_list()
