/* config.c - configuration management
 *
 * Copyright (C) 2024 Dwi Asmoro Bangun <dwiaceromo@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <cairo.h>
#include <libinput.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "cwc/config.h"
#include "cwc/util.h"

void cwc_config_init()
{
    cwc_config_set_default();
    g_config.old_config = malloc(sizeof(struct cwc_config));
    memcpy(g_config.old_config, &g_config, sizeof(g_config));
    wl_signal_init(&g_config.events.commit);
}

void cwc_config_commit()
{
    cwc_log(CWC_INFO, "config committed");
    wl_signal_emit(&g_config.events.commit, g_config.old_config);
    memcpy(g_config.old_config, &g_config, sizeof(g_config));
}

void cwc_config_set_default()
{
    g_config.tasklist_show_all            = true;
    g_config.border_color_rotation_degree = 0;
    g_config.border_color_focus           = NULL;
    g_config.border_color_normal =
        cairo_pattern_create_rgba(127 / 255.0, 127 / 255.0, 127 / 255.0, 1);
    g_config.useless_gaps = 0;
    g_config.border_width = 1;

    g_config.cursor_size                           = 24;
    g_config.cursor_inactive_timeout_ms            = 5000;
    g_config.cursor_edge_threshold                 = 16;
    g_config.cursor_edge_snapping_overlay_color[0] = 0.1;
    g_config.cursor_edge_snapping_overlay_color[1] = 0.2;
    g_config.cursor_edge_snapping_overlay_color[2] = 0.4;
    g_config.cursor_edge_snapping_overlay_color[3] = 0.1;

    g_config.repeat_rate  = 30;
    g_config.repeat_delay = 400;
    g_config.xkb_rules    = NULL;
    g_config.xkb_model    = NULL;
    g_config.xkb_layout   = NULL;
    g_config.xkb_variant  = NULL;
    g_config.xkb_options  = NULL;
}

void cwc_config_set_cairo_pattern(cairo_pattern_t **dst, cairo_pattern_t *src)
{
    if (g_config.border_color_focus)
        cairo_pattern_destroy(*dst);

    *dst = cairo_pattern_reference(src);
}

void cwc_config_set_number_positive(int *dest, int src)
{
    *dest = MAX(0, src);
}
