local cful = require("cuteful")
local bit = require("bit")

local mod = cful.enum.modifier
local cwc = cwc
local objname = "cwc_tablet"

local signal_list = {
    "tablet::new",
    "tablet::destroy",
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
        print(string.format("%d of %d %s signal test \27[1;31mFAILED\27[0m", count, #signal_list,
            objname))
    else
        print(string.format("%s signal test \27[1;32mPASSED\27[0m", objname))
    end
end

local function ro_test(tablet)
    assert(type(tablet.width) == "number")
    assert(type(tablet.height) == "number")
    assert(type(tablet.usb_product_id) == "number")
    assert(type(tablet.usb_vendor_id) == "number")
end

local function method_test(tablet)
    local screen = cwc.screen.get()[1]

    tablet:map_to_output(screen)
    tablet:map_to_region {
        x = 10,
        y = 30,
        width = 1000,
        height = 1000,
    }
end

local function test()
    local tablet = cwc.tablet.get()[1]

    ro_test(tablet)
    method_test(tablet)

    print(string.format("%s test \27[1;32mPASSED\27[0m", objname))
end

return {
    api = test,
    signal = signal_check,
}
