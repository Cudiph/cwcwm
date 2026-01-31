---------------------------------------------------------------------------
--- Default implementation that optionally enabled.
--
--
-- - `impl.border` - Manage the client border color based on it states.
-- - `impl.default_behavior` - Common behavior that found in other window manager/wayland compositor
-- - `impl.default_keybind` - The default keybind
-- - `impl.default_mousebind`- The default mousebind
--
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

--- Run the default behavior and binding script.
--
-- @staticfct use_default
-- @noreturn
local function use_default()
    require("impl.default_behavior")
    require("impl.default_keybind")
    require("impl.default_mousebind")
end

return {
    use_core = use_core,
    use_default = use_default,
}
