local gears = require("gears")
local enum = require("cuteful.enum")

__cwc_config = {
    tasklist_show_all                  = true,

    border_color_rotation              = 0,
    border_width                       = 1,
    default_decoration_mode            = enum.decoration_mode.SERVER_SIDE,
    border_color_focus                 = gears.color("#888888"),
    border_color_normal                = gears.color("#888888"),

    useless_gaps                       = 3,

    cursor_size                        = 24,
    cursor_inactive_timeout            = 5000,
    cursor_edge_threshold              = 16,
    cursor_edge_snapping_overlay_color = { 0.1, 0.2, 0.4, 0.1 },

    repeat_rate                        = 30,
    repeat_delay                       = 400,
    xkb_rules                          = "",
    xkb_model                          = "",
    xkb_layout                         = "",
    xkb_variant                        = "",
    xkb_options                        = "",
}
