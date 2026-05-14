local cful = require("cuteful")

local cwc = cwc
local devtype = cful.enum.device_type

local signal_list = {
    "input::new",
    "input::destroy",
}

local triggered_list = {}

for _, signame in pairs(signal_list) do
    cwc.connect_signal(signame, function()
        triggered_list[signame] = true
    end)
end

local function signal_check()
    local count = 0
    for _, signame in pairs(signal_list) do
        if not triggered_list[signame] then
            local template = string.format("signal %s is not triggered", signame)
            print(template)
            count = count + 1
        end
    end

    if count > 0 then
        print(string.format("%d of %d cwc_input signal test \27[1;31mFAILED\27[0m", count,
            #signal_list))
    else
        print("cwc_input signal test \27[1;32mPASSED\27[0m")
    end
end

local function property_test(dev)
    assert(type(dev.type) == "number")
    assert(type(dev.name) == "string")
    assert(type(dev.sysname) == "string")
    assert(type(dev.output_name) == "string" or dev.output_name == nil)
    assert(type(dev.id_vendor) == "number")
    assert(type(dev.id_bustype) == "number")
    assert(type(dev.id_product) == "number")
    assert(type(dev.send_events_mode) == "number")
    assert(type(dev.left_handed) == "boolean")
    assert(type(dev.sensitivity) == "number")
    assert(type(dev.accel_profile) == "number")
    assert(type(dev.natural_scroll) == "boolean")
    assert(type(dev.middle_emulation) == "boolean")
    assert(type(dev.rotation_angle) == "number")
    assert(type(dev.tap) == "boolean")
    assert(type(dev.tap_drag) == "boolean")
    assert(type(dev.tap_drag_lock) == "number")
    assert(type(dev.click_method) == "number")
    assert(type(dev.scroll_method) == "number")
    assert(type(dev.dwt) == "boolean")
end



local function method_test(dev)
    if dev.type == devtype.POINTER or dev.type == devtype.TABLET or dev.type == devtype.TOUCH then
        local test1 = dev:map_to_output(cwc.screen.get()[1])
        assert(test1 == true)

        local test2 = dev:map_to_region("not a table")
        assert(test2 == false)
        local test3 = dev:map_to_region { x = 10, y = 10, width = 300, height = 400 }
        assert(test3 == true)
    end
end

local function test()
    for _, dev in ipairs(cwc.input.get()) do
        property_test(dev)
        method_test(dev)
    end

    print("cwc_input test \27[1;32mPASSED\27[0m")
end

return {
    api = test,
    signal = signal_check,
}
