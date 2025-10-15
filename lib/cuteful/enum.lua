---------------------------------------------------------------------------
--- Constants extracted from C code.
--
-- @author Dwi Asmoro Bangun
-- @copyright 2024
-- @license GPLv3
-- @module cuteful.enum
---------------------------------------------------------------------------

local bit = require("bit")

local enum = {

    --- Keyboard modifier constant mapped from `wlr_keyboard.h`
    --
    -- @table modifier
    modifier = {
        NONE  = 0,
        SHIFT = bit.lshift(1, 0),
        CAPS  = bit.lshift(1, 1),
        CTRL  = bit.lshift(1, 2),
        ALT   = bit.lshift(1, 3),
        MOD2  = bit.lshift(1, 4),
        MOD3  = bit.lshift(1, 5),
        LOGO  = bit.lshift(1, 6), -- Aka super/mod4/window key
        MOD5  = bit.lshift(1, 7),
    },

    --- Extracted from Linux `input-event-codes.h`
    --
    -- @table mouse_btn
    mouse_btn = {
        LEFT         = 0x110,
        RIGHT        = 0x111,
        MIDDLE       = 0x112,
        SIDE         = 0x113,
        EXTRA        = 0x114,
        FORWARD      = 0x115,
        BACK         = 0x116,
        TASK         = 0x117,

        -- pseudo button this doesn't exist in linux input event codes header
        SCROLL_LEFT  = 0x13371,
        SCROLL_UP    = 0x13372,
        SCROLL_RIGHT = 0x13373,
        SCROLL_DOWN  = 0x13374,
    },

    --- Extracted from wlr_direction `wlr_output_layout.h`
    --
    -- @table direction
    direction = {
        UP    = bit.lshift(1, 0),
        DOWN  = bit.lshift(1, 1),
        LEFT  = bit.lshift(1, 2),
        RIGHT = bit.lshift(1, 3),
    },

    --- Extracted from `wayland-server-protocol.h`
    --
    -- @table output_transform
    output_transform = {
        TRANSFORM_NORMAL      = 0,
        TRANSFORM_90          = 1,
        TRANSFORM_180         = 2,
        TRANSFORM_270         = 3,
        TRANSFORM_FLIPPED     = 4,
        TRANSFORM_FLIPPED_90  = 5,
        TRANSFORM_FLIPPED_180 = 6,
        TRANSFORM_FLIPPED_270 = 7,
    },

    --- Input device type extracted from `wlr-input-device.h`
    --
    -- @table device_type
    device_type = {
        KEYBOARD   = 0,
        POINTER    = 1,
        TOUCH      = 2,
        TABLET     = 3,
        TABLET_PAD = 4,
        SWITCH     = 5,
    },

    --- Pointer constant used for configuring pointer device from `libinput.h`
    -- Ref: <https://wayland.freedesktop.org/libinput/doc/latest/api/group__config.html>
    --
    --@table libinput
    libinput = {
        SCROLL_NO_SCROLL                       = 0,
        SCROLL_2FG                             = bit.lshift(1, 0),
        SCROLL_EDGE                            = bit.lshift(1, 1),
        SCROLL_ON_BUTTON_DOWN                  = bit.lshift(1, 2),

        CLICK_METHOD_NONE                      = 0,
        CLICK_METHOD_BUTTON_AREAS              = bit.lshift(1, 0),
        CLICK_METHOD_CLICKFINGER               = bit.lshift(1, 1),

        SEND_EVENTS_ENABLED                    = 0,
        SEND_EVENTS_DISABLED                   = bit.lshift(1, 0),
        SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE = bit.lshift(1, 1),

        ACCEL_PROFILE_FLAT                     = bit.lshift(1, 0),
        ACCEL_PROFILE_ADAPTIVE                 = bit.lshift(1, 1),
        ACCEL_PROFILE_CUSTOM                   = bit.lshift(1, 2),

        TAP_MAP_LRM                            = 0,
        TAP_MAP_LMR                            = 1,

        DRAG_LOCK_DISABLED                     = 0,
        DRAG_LOCK_ENABLED_TIMEOUT              = 1,
        DRAG_LOCK_ENABLED_STICKY               = 2,

    },

    --- layout_mode enum extracted from cwc `types.h`.
    --
    -- @table layout_mode
    layout_mode = {
        FLOATING = 0,
        MASTER   = 1,
        BSP      = 2,
        LENGTH   = 3,
    },

    --- toplevel decoration mode enum extracted from cwc `toplevel.h`.
    --
    -- @table decoration_mode
    decoration_mode = {
        NONE                    = 0,
        CLIENT_SIDE             = 1,
        SERVER_SIDE             = 2,
        CLIENT_PREFERRED        = 100,
        CLIENT_SIDE_ON_FLOATING = 101,
    },

    --- content type enum extracted from `content-type-v1.xml` protocol.
    --
    -- @table content_type
    content_type = {
        NONE  = 0,
        PHOTO = 1,
        VIDEO = 2,
        GAME  = 3,
    },


}

return enum
