---------------------------------------------------------------------------
--- Default implementation that optionally enabled.
--
-- `impl.border` - Manage the client border color based on it states.
--
-- @author Dwi Asmoro Bangun
-- @copyright 2025
-- @license GPLv3
-- @module impl
---------------------------------------------------------------------------

--- Use implementation that provide core functionality.
--
-- @staticfct use_core
-- @noreturn
local function use_core()
    require("impl.border")
end

return {
    use_core = use_core
}
