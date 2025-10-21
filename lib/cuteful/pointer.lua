-------------------------------------------------------------------------------
--- Useful functions for pointer stuff.
--
-- @author Dwi Asmoro Bangun
-- @copyright 2025
-- @license GPLv3
-- @module cuteful.pointer
-------------------------------------------------------------------------------

local enum_dir = require("cuteful.enum").direction
local cwc = cwc

local pointer = {}

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
local already_listened = false
local function listen_init()
    if already_listened then return end

    local range_from_initial_pos = 0
    local current_bind = nil
    local initial_update = true
    local direction = nil
    local committed = false
    local COMMIT_THRESHOLD = 200

    cwc.connect_signal("pointer::swipe::begin", function(pointer, msec, fingers)
        range_from_initial_pos = 0
        initial_update = true
        direction = nil
        current_bind = nil
        committed = false
    end)

    cwc.connect_signal("pointer::swipe::update", function(pointer, msec, fingers, dx, dy)
        if initial_update then
            initial_update = false

            direction = get_direction_from_xy(dx, dy)
            current_bind = bind_info[generate_swipe_bind_info_key(fingers, direction)]
        end

        print(direction, range_from_initial_pos, dx, range_from_initial_pos + dx)

        if direction == enum_dir.LEFT or direction == enum_dir.RIGHT then
            range_from_initial_pos = range_from_initial_pos + dx
        else
            range_from_initial_pos = range_from_initial_pos + dy
        end

        if not current_bind then return end

        local CANCEL_THRESHOLD = 50
        if committed then
            if math.abs(range_from_initial_pos) < CANCEL_THRESHOLD then
                current_bind.cancelled()
                committed = false
            end

            return
        end


        if (direction == enum_dir.LEFT or direction == enum_dir.UP) and range_from_initial_pos < -COMMIT_THRESHOLD or
            (direction == enum_dir.RIGHT or direction == enum_dir.DOWN) and range_from_initial_pos > COMMIT_THRESHOLD
        then
            current_bind.committed()
            committed = true
        end
    end)

    cwc.connect_signal("pointer::swipe::end", function(pointer, msec, cancelled)
        if committed and cancelled and current_bind then
            current_bind.cancelled()
        end
    end)

    already_listened = true
end

--- Get a screen by its relative index.
--
-- @staticfct bind_swipe
-- @tparam integer fingers Number of fingers that touch the surface.
-- @tparam enum direction The swipe direction.
-- @tparam function committed Callback function when swipe is considered valid.
-- @tparam function cancelled Callback function when swipe is considered not swiping.
-- @tparam table options.
-- @tparam integer options.threshold Threshold of
function pointer.bind_swipe(fingers, direction, f_committed, f_cancelled)
    listen_init()
    bind_info[generate_swipe_bind_info_key(fingers, direction)] = {
        committed = f_committed,
        cancelled = f_cancelled,
    }
end

function pointer.unbind_swipe(fingers, direction)
    bind_info[generate_swipe_bind_info_key(fingers, direction)] = nil
end

return pointer
