local cwc = cwc
local objname = "cwc_pointer"

local signal_list = {
    -- "pointer::move",
    -- "pointer::button",
    -- "pointer::move",
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
        print(string.format("%d of %d %s signal test \27[1;31mFAILED\27[0m", count, #signal_list, objname))
    else
        print(string.format("%s signal test \27[1;32mPASSED\27[0m", objname))
    end
end

local function ro_test(pointer)
    assert(pointer.seat == "seat0")
end

local function prop_test(pointer)
    assert(type(pointer.position) == "table")
    assert(type(pointer.position.x) == "number")
    assert(type(pointer.position.y) == "number")

    pointer.position = { x = 50, y = 100 }
    assert(pointer.position.x == 50)
    assert(pointer.position.y == 100)

    assert(pointer.grab == false)
    pointer.grab = not pointer.grab
    assert(pointer.grab)

    assert(pointer.send_events)
    pointer.send_events = not pointer.send_events
    assert(pointer.send_events == false)
end

local function method_test(pointer)
    pointer:move(100, 100)
    pointer:move(-10, -10, true)

    pointer:move_to(300, 300)
    pointer:move_to(1e9, 1e9, true)
end

local function test()
    local pointer = cwc.pointer.get()[1]

    ro_test(pointer)
    prop_test(pointer)
    method_test(pointer)

    print(string.format("%s test \27[1;32mPASSED\27[0m", objname))
end

return {
    api = test,
    signal = signal_check,
}
