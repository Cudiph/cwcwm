local crules = require("cuteful.rules")
local check_rule = crules.check_rule

local sample_obj = {
    appid = "test object",
    workspace = 5,
    set_workspace = function(obj, ws)
        obj.workspace = ws
    end,
}

assert(check_rule(sample_obj, {
    where = { appid = "test object", workspace = 5 },
}))
assert(false == check_rule(sample_obj, {
    where = { appid = "test object", workspace = 3 },
}))
assert(check_rule(sample_obj, {
    where = { appid = "object", workspace = 5 },
}))
assert(false == check_rule(sample_obj, {
    where = { appid = "^object$", workspace = 5 },
}))

assert(check_rule(sample_obj, {
    where_any = { appid = { "diff", "test object" }, workspace = { 1 } },
}))
assert(false == pcall(check_rule, sample_obj, {
    where_any = { appid = "test object", workspace = 5 },
}))
assert(check_rule(sample_obj, {
    where_any = { appid = { "test object" } },
}))
assert(false == check_rule(sample_obj, {
    where_any = { appid = { "^object$" }, workspace = { 1 } },
}))

assert(check_rule(sample_obj, {
    where_not = { appid = "diff", workspace = 3 },
}))
assert(false == check_rule(sample_obj, {
    where_not = { appid = "test object", workspace = 5 },
}))
assert(false == check_rule(sample_obj, {
    where_not = { appid = "test object", workspace = 3 },
}))
assert(check_rule(sample_obj, {
    where_not = { appid = "diff", aaa = nil },
}))

assert(false == pcall(check_rule, sample_obj, {
    where_not_any = { appid = "diff", aaa = nil },
}))
assert(false == check_rule(sample_obj, {
    where_not_any = { appid = { "diff", "test object" }, workspace = { 3, 6 } },
}))
assert(false == check_rule(sample_obj, {
    where_not_any = { appid = { "diff" }, workspace = { 3, 5 } },
}))

local id = crules.add_rule {
    where = { appid = "any" },
    set = { tag = 5 },
    when = { "client::map" },
}

crules.remove_rule(id)

print(string.format("%s test \27[1;32mPASSED\27[0m", debug.getinfo(1, "S").source))
