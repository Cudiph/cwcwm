local gears = require("gears")
local enum = require("cuteful.enum")

local conf = {
    -- misc --
    tasklist_show_all                  = false,

    -- pointer config --
    cursor_size                        = 20,
    cursor_inactive_timeout            = 5000,
    cursor_edge_threshold              = 32,
    cursor_edge_snapping_overlay_color = { 0.1, 0.2, 0.3, 0.05 },

    -- keyboard config --
    repeat_rate                        = 30,
    repeat_delay                       = 300,
    -- xkb_variant                        = "colemak",
    -- xkb_layout                         = "us,de,fr",
    -- xkb_options                        = "grp:alt_shift_toggle,grp:caps_select",

    -- client config --
    default_decoration_mode            = enum.decoration_mode.SERVER_SIDE,
    border_width                       = 1,
    border_color_rotation              = 64,
    border_color_focus                 = gears.color(
        "linear:0,0:0,0:0,#f08e97:0.1,#a7e1a4:0.2,#ffffa7:0.3,#a5c0e1:0.4,#c8a6e1:0.5,#a1d0d4:0.6,#f9b486:0.7,#e1a5d7:0.8,#b4b8e6:0.9,#b4b8e6:1.0,#f8e0b4"),
    border_color_normal                = gears.color("#423e44"),
    border_color_raised                = gears.color("#d2d6f9"),

    -- screen/tag config --
    useless_gaps                       = 3,
}

return conf
