local cwc = cwc
local objname = "cwc_kbd"

local signal_list = {
    -- "kbd::press",
    -- "kbd::release", }

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
        print(string.format("%d of %d %s signal test \27[1;31mFAILED\27[0m", count, #signal_list, objname))
    else
        print(string.format("%s signal test \27[1;32mPASSED\27[0m", objname))
    end
end

local function ro_test(kbd)
    assert(kbd.seat == "seat0")
    assert(kbd.modifiers == 0)
end

local function prop_test(kbd)
    assert(not kbd.grab)
    kbd.grab = true
    assert(kbd.grab)
    kbd.grab = false

    assert(kbd.send_events)
    kbd.send_events = false
    assert(not kbd.send_events)
    kbd.send_events = true
end

local function method_test(kbd)
end

local function test()
    local kbd = cwc.kbd.get()[1]

    ro_test(kbd)
    prop_test(kbd)
    method_test(kbd)

    print(string.format("%s test \27[1;32mPASSED\27[0m", objname))
end

return {
    api = test,
    signal = signal_check,
}
