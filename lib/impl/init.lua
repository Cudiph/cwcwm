---------------------------------------------------------------------------
--- Default implementation that can be disabled.
--
-- `impl.border` - Manage the client border color based on it states.
--
-- @author Dwi Asmoro Bangun
-- @copyright 2025
-- @license GPLv3
-- @module impl
---------------------------------------------------------------------------

--- Use all implementation available.
--
-- @staticfct use_all
-- @noreturn
local function use_all()
    require("impl.border")
end

return {
    use_all = use_all
}
