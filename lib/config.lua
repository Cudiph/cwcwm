---------------------------------------------------------------------------
--- High level interface to the private `__cwc_config` global table.
--
-- any write on the config table will automatically be taken care of,
-- so no need to manually call `cwc.commit`.
--
-- @author Dwi Asmoro Bangun
-- @copyright 2025
-- @license GPLv3
-- @module config
---------------------------------------------------------------------------

local enum           = require("cuteful.enum")
local gears          = require("gears")
local gtable         = gears.table
local protected_call = gears.protected_call
local print_error    = gears.debug.print_error

local cwc            = cwc
local __cwc_config   = __cwc_config
local config         = { mt = {} }

--- Merge source table to the config table.
--
-- @tparam string|table src The configuration table to load. It can be either the path to the config
-- file (which should return a table) or directly a table containing all the config value.
-- @treturn boolean True if successful, false otherwise.
-- @staticfct init
function config.init(src)
    if type(src) == "table" then
        gtable.crush(config, src)
        cwc.commit()
        return true
    end

    if type(src) == "string" then
        local result = protected_call(dofile, src)
        if not result or type(result) ~= "table" then
            print_error("config: the resulted file doesn't return a table")
            return false
        end

        gtable.crush(config, result)
        cwc.commit()
        return true
    end

    print_error("config: source is not a table nor a string")
    return false
end

--- Check if item is created from `gears.color`.
--
-- @staticfct color_check
-- @tparam gears_color item Item to check.
-- @treturn boolean
function config.check_color(item)
    if tostring(item):match("cairo") then return true end

    return false
end

--- Check if item is a positive number.
--
-- @staticfct positive
-- @tparam number item Item to check.
-- @treturn boolean
function config.check_positive(item)
    if item >= 0 then return true end

    return false
end

--- Check if item is a positive number.
--
-- @staticfct check_enum
-- @tparam number item Item to check.
-- @tparam table enum_table The enum table.
-- @treturn boolean
function config.check_enum(enum_table)
    return function(item)
        for _, pair in pairs(enum_table) do
            if item == pair then return true end
        end

        return false
    end
end

--- The color of client border
-- @config border_color_normal
-- @param gears_color

--- The color of fcoused client border
-- @config border_color_focus
-- @param gears_color

--- The size of the cursor
-- @config cursor_size
-- @param integer

--- The gap size between clients
-- @config useless_gaps
-- @param integer

-- sanity check table either a string (the type) or a function that return a boolean that determine if
-- it pass the sanity check (true) or not (false)
local sanity_check = {
    tasklist_show_all                  = "boolean",

    border_color_normal                = config.check_color,
    border_color_focus                 = config.check_color,
    border_color_rotation_degree       = "number",
    border_width                       = config.check_positive,
    default_decoration_mode            = config.check_enum(enum.decoration_mode),

    useless_gaps                       = config.check_positive,

    cursor_size                        = config.check_positive,
    cursor_inactive_timeout            = config.check_positive,
    cursor_edge_threshold              = config.check_positive,
    cursor_edge_snapping_overlay_color = "table", -- TODO: proper checking

    repeat_rate                        = config.check_positive,
    repeat_delay                       = config.check_positive,
    xkb_variant                        = "string",
    xkb_layout                         = "string",
    xkb_options                        = "string",
}

--- Check if item is a positive number.
--
-- @staticfct sanity_check_add
-- @tparam string key Config key to add.
-- @tparam function|string checker Function or data type as a tester.
-- @noreturn
function config.sanity_check_add(key, checker)
    if sanity_check[key] then
        return print_error("config: sanity check for `" .. key .. "` already exist")
    end

    sanity_check[key] = checker
end

local is_committed = false
local function delayed_commit()
    if is_committed then return end

    cwc.timer.delayed_call(function()
        cwc.commit()
        is_committed = false
    end)

    is_committed = true
end

config.mt.__index    = function(t, k)
    return rawget(__cwc_config, k)
end

config.mt.__newindex = function(t, k, v)
    local sanity_val = sanity_check[k]

    local sanity_type = type(sanity_val)

    if (sanity_type == "string" and type(v) ~= sanity_val) or
        (sanity_type == "function" and not sanity_val(v)) then
        print_error("config: key " .. k .. " is not using the right type")
        return
    end

    rawset(__cwc_config, k, v)
    delayed_commit()
end

return setmetatable(config, config.mt)
