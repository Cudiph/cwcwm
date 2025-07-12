local gstring = require("gears.string")
local cwc = cwc

local function on_client_custom(c, testname, optarg)
    assert(gstring.startswith(tostring(c), "cwc_client"))
    assert(testname == "sig1" or testname == "sig2")
    assert(100 or nil)

    print(string.format("lua signal test %s \27[1;32mPASSED\27[0m with received value:", testname), c, testname,
        optarg)
end

local function on_client_map(c)
    assert(gstring.startswith(tostring(c), "cwc_client"))
    print("lua client map signal \27[1;32mPASSED\27[0m", c)
end

local function on_client_unmap(c)
    assert(gstring.startswith(tostring(c), "cwc_client"))
    print("lua client unmap signal \27[1;32mPASSED\27[0m", c)
end

local function test()
    cwc.connect_signal("client::map", on_client_map)
    cwc.connect_signal("client::unmap", on_client_unmap)

    local clients = cwc.screen.focused():get_clients()
    cwc.connect_signal("client::custom", on_client_custom)
    cwc.emit_signal("client::custom", clients[1], "sig1")
    cwc.emit_signal("client::custom", clients[1], "sig2", 100)
end

return test
