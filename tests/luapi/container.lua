local cwc = cwc

local signal_list = {
    "container::new",
    "container::destroy",
    "container::insert",
    "container::remove",
    "container::swap",
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
        print(string.format("%d cwc_container signal test FAILED", count))
    else
        print("cwc_container signal test PASSED")
    end
end

local function property_test(cont, c)
    assert(type(cont.clients) == "table")
    assert(string.find(tostring(cont.clients[1]), "cwc_client"))
    assert(cont.front == c)

    local geom = cont.geometry
    assert(type(cont.geometry) == "table")
    assert(type(geom) == "table")
    assert(type(geom.x) == "number")
    assert(type(geom.y) == "number")
    assert(type(geom.width) == "number")
    assert(type(geom.height) == "number")

    assert(cont.insert_mark == false)
    cont.insert_mark = true
    assert(cont.insert_mark == true)
end

local function method_test(cont)
    local cts = cwc.container.get()
    local rand_cont = cts[math.random(#cts)]
    print(rand_cont)
    cont:focusidx(1)
    cont:swap(rand_cont)
    assert(#cont.client_stack == #cont:get_client_stack(true))
end

local function test()
    local c = cwc.client.focused()
    local cont = c.container

    property_test(cont, c)
    method_test(cont)

    print("cwc_container test PASSED")
end

return {
    api = test,
    signal = signal_check,
}
