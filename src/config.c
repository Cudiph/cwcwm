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
#include <lua.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_primary_selection.h>

#include "cwc/config.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/input/keyboard.h"
#include "cwc/input/manager.h"
#include "cwc/input/seat.h"
#include "cwc/luac.h"
#include "cwc/server.h"
#include "cwc/util.h"
#include "lauxlib.h"

static struct wl_listener on_commit_l;

static void _clear_all_primary_selection()
{
    struct cwc_seat *seat;
    wl_list_for_each(seat, &server.input->seats, link)
    {
        wlr_seat_set_primary_selection(
            seat->wlr_seat, NULL, wl_display_next_serial(server.wl_display));
    }
}

#define UPDATE_XKB_OPTIONS(opt_name)                               \
    if (luaC_config_get(L, "xkb_" #opt_name)) {                    \
        free(g_config.xkb_##opt_name);                             \
        g_config.xkb_##opt_name = strdup(luaL_checkstring(L, -1)); \
        update_xkb_idle();                                         \
    }

static void on_commit(struct wl_listener *listener, void *data)
{
    lua_State *L = g_config_get_lua_State();

    if (luaC_config_get(L, "tasklist_show_all"))
        g_config.tasklist_show_all = lua_toboolean(L, -1);
    if (luaC_config_get(L, "middle_click_paste")) {
        g_config.middle_click_paste = lua_toboolean(L, -1);
        if (!g_config.middle_click_paste)
            _clear_all_primary_selection();
    }

    if (luaC_config_get(L, "border_color_rotation"))
        g_config.border_color_rotation = lua_tointeger(L, -1);
    if (luaC_config_get(L, "border_width"))
        g_config.border_width = lua_tointeger(L, -1);
    if (luaC_config_get(L, "default_decoration_mode"))
        g_config.default_decoration_mode = lua_tointeger(L, -1);

    if (luaC_config_get(L, "useless_gaps")) {
        g_config.useless_gaps = lua_tointeger(L, -1);
    }

    if (luaC_config_get(L, "cursor_size"))
        g_config.cursor_size = lua_tointeger(L, -1);
    if (luaC_config_get(L, "cursor_inactive_timeout"))
        g_config.cursor_inactive_timeout = lua_tointeger(L, -1);
    if (luaC_config_get(L, "cursor_edge_threshold"))
        g_config.cursor_edge_threshold = lua_tointeger(L, -1);
    if (luaC_config_get(L, "cursor_edge_snapping_overlay_color")) {
        for (int i = 0; i < 3; i++) {
            lua_rawgeti(L, -1, i + 1);
            g_config.cursor_edge_snapping_overlay_color[i] =
                lua_tonumber(L, -1);
            lua_pop(L, 1);
        }
    }

    if (luaC_config_get(L, "repeat_rate"))
        g_config.repeat_rate = lua_tointeger(L, -1);
    if (luaC_config_get(L, "repeat_delay"))
        g_config.repeat_delay = lua_tointeger(L, -1);
    UPDATE_XKB_OPTIONS(rules)
    UPDATE_XKB_OPTIONS(model)
    UPDATE_XKB_OPTIONS(layout)
    UPDATE_XKB_OPTIONS(variant)
    UPDATE_XKB_OPTIONS(options)
}

void cwc_config_init()
{
    cwc_config_set_default();
    g_config.old_config = malloc(sizeof(struct cwc_config));
    memcpy(g_config.old_config, &g_config, sizeof(g_config));
    wl_signal_init(&g_config.events.commit);

    on_commit_l.notify = on_commit;
    wl_signal_add(&g_config.events.commit, &on_commit_l);
}

void cwc_config_commit()
{
    cwc_log(CWC_INFO, "config committed");
    wl_signal_emit(&g_config.events.commit, g_config.old_config);
    memcpy(g_config.old_config, &g_config, sizeof(g_config));
}

void cwc_config_set_default()
{
    g_config.tasklist_show_all  = true;
    g_config.middle_click_paste = true;

    g_config.border_color_rotation   = 0;
    g_config.useless_gaps            = 0;
    g_config.border_width            = 1;
    g_config.default_decoration_mode = CWC_TOPLEVEL_DECORATION_SERVER_SIDE;

    g_config.cursor_size                           = 24;
    g_config.cursor_inactive_timeout               = 5000;
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

void cwc_config_set_number_positive(int *dest, int src)
{
    *dest = MAX(0, src);
}
