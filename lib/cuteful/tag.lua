---------------------------------------------------------------------------
--- Useful functions for tag operation.
--
-- @author Dwi Asmoro Bangun
-- @copyright 2024
-- @license GPLv3
-- @module cuteful.tag
---------------------------------------------------------------------------

local gears = require("gears")
local gtable = gears.table
local cwc = cwc

local tag = {
    history = {},
}

--- Select a tag relative to the currently selected one will cycle between the general workspace range.
--
-- @staticfct cuteful.tag.viewidx
-- @tparam number idx View index relative to the current tag
-- @tparam[opt] cwc_screen screen The screen
-- @noreturn
function tag.viewidx(idx, screen)
    local s = screen or cwc.screen.focused()

    local active_ws = s.active_workspace - 1
    active_ws = (active_ws + idx) % s.max_general_workspace
    s.active_workspace = active_ws + 1
end

--- View next tag
--
-- @staticfct cuteful.tag.viewnext
-- @tparam[opt] cwc_screen screen
-- @noreturn
function tag.viewnext(screen)
    tag.viewidx(1, screen)
end

--- View previous tag
--
-- @staticfct cuteful.tag.viewprev
-- @tparam[opt] cwc_screen screen
-- @noreturn
function tag.viewprev(screen)
    tag.viewidx(-1, screen)
end

--- View no tag.
--
-- @staticfct viewnone
-- @tparam[opt] cwc_screen screen
-- @noreturn
function tag.viewnone(screen)
    local s = screen or cwc.screen.focused()
    s.active_workspace = 0;
end

--- Set layout_mode for tag n.
--
-- @staticfct layout_mode
-- @tparam integer n
-- @tparam integer layout_mode
-- @tparam[opt] cwc_screen screen
-- @noreturn
-- @see cuteful.enum.layout_mode
function tag.layout_mode(idx, layout_mode, screen)
    local s = screen or cwc.screen.focused()
    local t = s:get_tag(idx)

    t.layout_mode = layout_mode
end

--- Increase gap.
--
-- @staticfct incgap
-- @tparam integer add
-- @tparam[opt] cwc_screen screen
-- @noreturn
function tag.incgap(add, screen)
    local s = screen or cwc.screen.focused()
    local t = s.selected_tag
    t.useless_gaps = t.useless_gaps + add
end

--- Increase master width factor.
--
-- @staticfct incmwfact
-- @tparam number add
-- @tparam[opt] cwc_screen screen
-- @noreturn
function tag.incmwfact(add, screen)
    local s = screen or cwc.screen.focused()
    local t = s.selected_tag
    t.mwfact = t.mwfact + add
end

------------------ HISTORY -------------------

-- the end of the array is the top of the stack
local stacks = {}

cwc.connect_signal("screen::new", function(s)
    if not stacks[s.name] then
        stacks[s.name] = { 1 }
    end
end)

cwc.connect_signal("screen::prop::active_tag", function(s)
    local sel_stack = stacks[s.name]
    local k = gtable.hasitem(sel_stack, s.active_tag)

    if k then
        table.remove(sel_stack, k)
    end

    table.insert(sel_stack, s.active_tag)
end)

--- Revert tag history.
--
-- @staticfct cuteful.tag.history.restore
-- @tparam[opt] cwc_screen screen The screen.
-- @tparam[opt] integer idx Index in the history stack. Default to previous (idx number 1).
-- @noreturn
function tag.history.restore(screen, idx)
    local s = screen or cwc.screen.focused()
    idx = idx or 1
    idx = idx + 1

    local sel_stack = stacks[s.name]
    local selected = sel_stack[idx]
    if not selected then return end

    -- make the newest at the start of the array
    local reversed = gtable.reverse(sel_stack)
    s.active_tag = reversed[idx]
end

return tag
