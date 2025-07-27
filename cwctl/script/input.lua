local cwc = cwc

local function input_list()
    local inputs = cwc.input.get()
    local out = ""

    for idx, inp in pairs(inputs) do
        if out then out = out .. "\n" end
        local template =
            "[%d] %s (%s):\n" ..
            "\tName: %s\n" ..
            "\tType: %s\n" ..
            "\tSystem name: %s\n" ..
            "\tOutput name: %s\n" ..
            "\tVendor ID: %s\n" ..
            "\tBus type ID: %s\n" ..
            "\tProduct ID: %s\n" ..
            "\tSend-event mode: %s\n" ..
            "\tLeft handed: %s\n" ..
            "\tSensitivity: %s\n" ..
            "\tAcceleration profile: %s\n" ..
            "\tNatural scroll: %s\n" ..
            "\tMiddle emulation: %s\n" ..
            "\tRotation angle: %s\n" ..
            "\tTap to click: %s\n" ..
            "\tTap and drag: %s\n" ..
            "\tTap and drag lock: %s\n" ..
            "\tClick method: %s\n" ..
            "\tScroll method: %s\n" ..
            "\tDisable while typing: %s\n" ..
            ""

        out = out .. string.format(template,
            idx, inp.name, inp,
            inp.name,
            inp.type,
            inp.sysname,
            inp.output_name,
            inp.id_vendor,
            inp.id_bustype,
            inp.id_product,
            inp.send_events_mode,
            inp.left_handed,
            inp.sensitivity,
            inp.accel_profile,
            inp.natural_scroll,
            inp.middle_emulation,
            inp.rotation_angle,
            inp.tap,
            inp.tap_drag,
            inp.tap_drag_lock,
            inp.click_method,
            inp.scroll_method,
            inp.dwt
        )
    end

    return out
end

return input_list()
