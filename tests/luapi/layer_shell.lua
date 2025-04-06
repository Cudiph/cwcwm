local cwc = cwc

local signal_list = {
    "layer_shell::new",
    "layer_shell::destroy",
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
        print(string.format("%d cwc_layer_shell signal test FAILED", count))
    else
        print("cwc_layer_shell signal test PASSED")
    end
end

local function readonly_test(ls)
    assert(ls.screen)
    assert(ls.namespace)
    assert(ls.pid)
end

local function method_test(ls)
    ls:kill()
end

local function test()
    local ls = cwc.layer_shell.get()[1]
    readonly_test(ls)
    method_test(ls)

    print("cwc_layer_shell test PASSED")
end


return {
    api = test,
    signal = signal_check,
}
