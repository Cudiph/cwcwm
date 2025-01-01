---------------------------------------------------------------------------
--- Useful functions for tag operation.
--
-- @author Dwi Asmoro Bangun
-- @copyright 2024
-- @license GPLv3
-- @module cuteful.tag
---------------------------------------------------------------------------

local cwc = cwc

local tag = {}

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

return tag
