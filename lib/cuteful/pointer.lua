-------------------------------------------------------------------------------
--- Useful functions for pointer stuff.
--
-- @author Dwi Asmoro Bangun
-- @copyright 2025
-- @license GPLv3
-- @module cuteful.pointer
-- @see cwc.kbd
-- @see cwc.pointer
-- @see cuteful.kbd
-------------------------------------------------------------------------------

local enum_dir = require("cuteful.enum").direction
local gears = require("gears")

local cwc = cwc
local pointer_api = {}

local function generate_swipe_bind_info_key(fingers, direction)
    return "swipe_" .. fingers .. "_" .. direction
end

local bind_info = {}


local function get_direction_from_xy(x, y)
    local absx = math.abs(x)
    local absy = math.abs(y)

    if absx > absy then
        if x >= 0 then
            return enum_dir.RIGHT
        else
            return enum_dir.LEFT
        end
    else
        if y >= 0 then
            return enum_dir.DOWN
        else
            return enum_dir.UP
        end
    end
end

-- listen only when bind is called to prevent adding key to signal map in C
local swipe_already_listened = false
local function swipe_listen_init()
    if swipe_already_listened then return end

    local range_from_initial_pos = 0
    local current_bind = nil
    local initial_update = true
    local direction = nil
    local committed = false
    local commit_threshold = 200
    local cancel_threshold = 50

    cwc.connect_signal("pointer::swipe::begin", function()
        range_from_initial_pos = 0
        initial_update = true
        direction = nil
        current_bind = nil
        committed = false
        commit_threshold = 200
        cancel_threshold = 50
    end)

    cwc.connect_signal("pointer::swipe::update", function(pointer, _, fingers, dx, dy)
        if initial_update then
            initial_update = false

            direction = get_direction_from_xy(dx, dy)
            current_bind = bind_info[generate_swipe_bind_info_key(fingers, direction)]

            if current_bind then
                commit_threshold = current_bind.threshold or commit_threshold
                cancel_threshold = current_bind.cancel_threshold or cancel_threshold
            end
        end

        if direction == enum_dir.LEFT or direction == enum_dir.RIGHT then
            range_from_initial_pos = range_from_initial_pos + dx
        else
            range_from_initial_pos = range_from_initial_pos + dy
        end

        if current_bind == nil then return end

        if current_bind.skip_events then
            pointer.send_events = false
            cwc.timer.delayed_call(function()
                pointer.send_events = true
            end)
        end

        if committed then
            if math.abs(range_from_initial_pos) < cancel_threshold then
                current_bind.cancelled(pointer)
                committed = false
            end

            return
        end

        if (direction == enum_dir.LEFT or direction == enum_dir.UP)
            and range_from_initial_pos < -commit_threshold
            or
            (direction == enum_dir.RIGHT or direction == enum_dir.DOWN)
            and range_from_initial_pos > commit_threshold
        then
            current_bind.committed(pointer)
            committed = true
        end
    end)

    swipe_already_listened = true
end

--- Get a screen by its relative index.
--
-- @staticfct bind_swipe
-- @tparam integer fingers Number of fingers that touch the surface.
-- @tparam enum direction The swipe direction.
-- @tparam function committed_cb Callback function when swipe is considered valid.
-- @tparam cwc_pointer committed_cb.pointer The pointer object.
-- @tparam function cancelled_cb Callback function when swipe is considered not swiping.
-- @tparam cwc_pointer cancelled_cb.pointer The pointer object.
-- @tparam table options
-- @tparam integer options.threshold Threshold distance from initial point that is considered a swipes.
-- @tparam integer options.cancel_threshold Threshold distance from initial point to be considered cancelled.
-- @tparam integer options.skip_events Don't send swipe gesturesevents to focused client.
-- @noreturn
function pointer_api.bind_swipe(fingers, direction, f_committed, f_cancelled, options)
    swipe_listen_init()

    if options and options.cancel_threshold and options.threshold and options.cancel_threshold >= options.threshold then
        gears.debug.print_error("Threshold is lower than cancel threshold")
    end

    bind_info[generate_swipe_bind_info_key(fingers, direction)] = {
        committed = f_committed,
        cancelled = f_cancelled,
        threshold = options and options.threshold,
        cancel_threshold = options and options.cancel_threshold,
        skip_events = options and options.skip_events,
    }
end

function pointer_api.unbind_swipe(fingers, direction)
    bind_info[generate_swipe_bind_info_key(fingers, direction)] = nil
end

return pointer_api
