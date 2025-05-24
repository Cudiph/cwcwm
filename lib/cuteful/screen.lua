-------------------------------------------------------------------------------
--- Useful functions for screen object.
--
-- @author Dwi Asmoro Bangun
-- @copyright 2025
-- @license GPLv3
-- @module cuteful.screen
-------------------------------------------------------------------------------

local cwc = cwc

local screen = {}

--- Move the focus to a screen relative to the current one.
--
-- @staticfct focus_relative
-- @tparam number offset Value to add to the current focused screen index. 1 to focus the next one, -1 to focus the previous one.
-- @noreturn
function screen.focus_relative(offset)
    local focused = cwc.screen.focused()
    local scrs = cwc.screen.get()

    local idx = 0
    for _, s in pairs(scrs) do
        if s == focused then break end
        idx = idx + 1
    end

    idx = (idx + offset) % #scrs + 1
    scrs[idx]:focus()
end

--- Move the focus to a screen in a specific direction.
--
-- @staticfct focus_bydirection
-- @tparam number dir The direction enum.
-- @tparam[opt=nil] cwc_screen s Screen.
-- @noreturn
-- @see cuteful.enum.direction
function screen.focus_bydirection(dir, s)
    s = s or cwc.screen.focused()
    s:get_nearest(dir):focus()
end

return screen
