-- Test the cwc_plugin property

local bit = require("bit")
local enum = require('cuteful.enum')

local cwc = cwc
local objname = "cwc_plugin"

local signal_list = {
    "plugin::load",
    "plugin::unload",
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
        print(string.format("%d of %d " .. objname .. " signal test \27[1;31mFAILED\27[0m", count, #signal_list))
    else
        print(objname .. " signal test \27[1;32mPASSED\27[0m")
    end
end

local function ro_test(p)
    assert(p.name == "dwl-ipc")
    assert(p.description == "dwl IPC plugin")
    assert(p.version:match("%d+%.%d+%.%d+"))
    assert(p.author == "Dwi Asmoro Bangun <dwiaceromo@gmail.com>")
    assert(p.license == "GPLv3")
    assert(p.filename:match(".*dwl%-ipc%.so$"))
end

local function method_test(p)
    p:unload()
end

local function test()
    local plugins_folder = cwc.is_nested() and "./build/plugins" or cwc.get_datadir() .. "/plugins"
    assert(cwc.plugin.load(plugins_folder .. "/cwcle.so"))
    assert(cwc.plugin.load(plugins_folder .. "/dwl-ipc.so"))

    local plugin = cwc.plugin.get()[1]

    ro_test(plugin)
    method_test(plugin)

    assert(cwc.plugin.unload_byname("cwcle"))

    print(objname .. " test \27[1;32mPASSED\27[0m")
end

return {
    api = test,
    signal = signal_check,
}
