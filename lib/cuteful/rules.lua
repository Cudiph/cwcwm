-------------------------------------------------------------------------------
--- Declarative way to rule objects.
--
-- The rule has 3 main components:
--
-- - **Constraint** - a set of condition the object has to satisfy. (`where` and `where_not` field)
-- - **Effect** - new value of the object if constraint is satisfied. (`set` and `run` field)
-- - **Event** - signals when the rule is applied. (`when` field)
--
-- Examples:
--
-- 1. Set picture-in-picture window to be floating and always on top.
--
--
--    -- the string comparison is using lua pattern matching and case insensitive
--    add_client_rule {
--        where = { title = "picture-*in-*picture" },
--        set = { ontop = true, floating = true },
--    }
--
--
-- 2. Set dolphin, thunar, pcmanfm to be always maximized.
--
--
--    add_client_rule {
--        where_any = {
--            appid = {
--               "dolphin",
--               "thunar",
--               "pcmanfm",
--            }
--        },
--        set = { maximized = true },
--    }
--
--
-- 3. Set every screen except DP-3 to use 1920x1080@240 mode and disallow tearing.
--
--
--    add_rule {
--      where_not = { name = "DP-3" },
--      set = { allow_tearing = false }
--      run = function (screen)
--          screen:set_mode(1920, 1080, 240)
--      end
--      when = { "screen::new" }
--    }
--
--
-- @author Dwi Asmoro Bangun
-- @copyright 2026
-- @license GPLv3
-- @module cuteful.rules
-------------------------------------------------------------------------------

local gears = require("gears")
local eprint = gears.debug.print_error

local cwc = cwc
local M = {}

-- Table structure:
--
-- local signal_map = {
--     ["client::map"] = {
--         <rule_table1>,
--         <rule_table2>,
--         ...
--     },
-- }
local signal_map = {}
local id_counter = 1

local function match_constraint_kv(obj, key, value)
    if type(value) == "string" then
        if tostring(obj[key]):lower():match(value) then
            return true
        end
    elseif obj[key] == value then
        return true
    end

    return false
end

function M.check_rule(obj, rule)
    -- constraints
    for const_key, const_val in pairs(rule.where or {}) do
        if not match_constraint_kv(obj, const_key, const_val) then
            return false
        end
    end

    if type(rule.where_any) == "table" then
        local match_any = false
        for const_key, const_vlist in pairs(rule.where_any) do
            for _, val in ipairs(const_vlist) do
                if match_constraint_kv(obj, const_key, val) then
                    match_any = true
                    goto matched
                end
            end
        end

        ::matched::
        if not match_any then return false end
    end

    for const_key, const_val in pairs(rule.where_not or {}) do
        if match_constraint_kv(obj, const_key, const_val) then
            return false
        end
    end

    if type(rule.where_not_any) == "table" then
        local match_any = false
        for const_key, const_vlist in pairs(rule.where_not_any) do
            for _, val in ipairs(const_vlist) do
                if match_constraint_kv(obj, const_key, val) then
                    match_any = true
                    goto matched
                end
            end
        end

        ::matched::
        if match_any then return false end
    end

    return true
end

--- Apply rule to an object.
--
-- @staticfct apply_rule
-- @tparam cwc_object obj The object to rule
-- @tparam table rule Rule table.
-- @tparam[opt=nil] table rule.where Constraint of the rule.
-- @tparam[opt=nil] table rule.where_any Constraint of the rule.
-- @tparam[opt=nil] table rule.where_not Constraint of the rule.
-- @tparam[opt=nil] table rule.where_not_any Constraint of the rule.
-- @tparam[opt=nil] table rule.set Effect of the rule if the constraint match.
-- @tparam[opt=nil] function rule.run Callback function if the constraint match.
-- @noreturn
function M.apply_rule(obj, rule)
    if not M.check_rule(obj, rule) then return end

    -- effect
    for effect_k, effect_v in pairs(rule.set or {}) do
        if obj["set_" .. effect_k] == nil then
            eprint("property " .. effect_k .. " is either read only or not exist")
            goto continue
        end

        obj[effect_k] = effect_v

        ::continue::
    end

    -- callback
    if rule.run then rule.run(obj) end
end

local function add_object_rule(sig, rule)
    if rule.set == nil and rule.run == nil then return error("set and run cannot be empty") end

    if signal_map[sig] == nil then
        signal_map[sig] = {}
        cwc.connect_signal(sig, function(obj)
            local saved_rules = signal_map[sig]
            for _, ruleset in ipairs(saved_rules) do
                M.apply_rule(obj, ruleset)
            end
        end)
    end

    table.insert(signal_map[sig], rule)
end

--- Create a rule for an object.
--
-- Either `set` or `run` field must be not empty.
--
-- @staticfct add_rule
-- @tparam table rule Rule table.
-- @tparam[opt=nil] table rule.where Constraint of the rule.
-- @tparam[opt=nil] table rule.where_any Constraint of the rule.
-- @tparam[opt=nil] table rule.where_not Constraint of the rule.
-- @tparam[opt=nil] table rule.where_not_any Constraint of the rule.
-- @tparam[opt=nil] table rule.set Effect of the rule if the constraint match.
-- @tparam[opt=nil] function rule.run Callback function if the constraint match.
-- @tparam table rule.when Signals to apply rule.
-- @treturn string The ID of the rule
function M.add_rule(rule)
    if rule.id == nil then
        rule.id = tostring(id_counter)
        id_counter = id_counter + 1
    end

    for _, sig in ipairs(rule.when) do
        add_object_rule(sig, rule)
    end

    return rule.id
end

--- Remove rule using id or table reference.
--
-- @staticfct remove_rule
-- @tparam table|string id The id or table reference.
-- @treturn boolean True if a rule has been removed.
function M.remove_rule(id)
    for _, sig in pairs(signal_map) do
        for i, rule in ipairs(sig) do
            if type(id) == "string" and rule.id == id or rule == id then
                table.remove(sig, i)
                return true
            end
        end
    end

    return false
end

--- Macro to create a rule that applied when a client appear.
--
-- @staticfct add_client_rule
-- @tparam table rule Rule table.
-- @treturn string The ID of the rule
-- @see add_rule
function M.add_client_rule(rule)
    rule.when = { "client::map" }
    return M.add_rule(rule)
end

return M
