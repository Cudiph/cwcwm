/* pointer.c - lua pointer object
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

/** Low-level API to manage pointer and pointer device
 *
 * @author Dwi Asmoro Bangun
 * @copyright 2024
 * @license GPLv3
 * @inputmodule cwc.pointer
 */

#include <drm_fourcc.h>
#include <lauxlib.h>
#include <linux/input-event-codes.h>
#include <lua.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_cursor_shape_v1.h>

#include "cwc/config.h"
#include "cwc/input/cursor.h"
#include "cwc/input/keyboard.h"
#include "cwc/input/manager.h"
#include "cwc/input/seat.h"
#include "cwc/luaclass.h"
#include "cwc/server.h"

/** Emitted when a pointer is moved or input device motion event is triggered.
 *
 * @signal pointer::move
 * @tparam cwc_pointer pointer The pointer object.
 * @tparam integer time_msec The event time in milliseconds.
 * @tparam number dx The x vector.
 * @tparam number dy The y vectork.
 * @tparam number dx_unaccel The x vector unaccelerated.
 * @tparam number dy_unaccel The y vector unaccelerated.
 */

/** Emitted when a mouse button is pressed/released.
 *
 * @signal pointer::button
 * @tparam cwc_pointer pointer The pointer object.
 * @tparam integer time_msec The event time in milliseconds.
 * @tparam integer button The button code from linux/input-event-codes.h.
 * @tparam boolean pressed The state of the button, `true` means pressed.
 * @see cuteful.enum.mouse_btn
 */

/** Emitted when an axis event is triggered.
 *
 * @signal pointer::axis
 * @tparam cwc_pointer pointer The pointer object.
 * @tparam integer time_msec The event time in milliseconds.
 * @tparam boolean horizontal The orientation of the axis.
 * @tparam number delta
 * @tparam number delta_discrete
 */

//============================ CODE =================================

/** Get all pointer in the server.
 *
 * @staticfct get
 * @treturn cwc_pointer[]
 */
static int luaC_pointer_get(lua_State *L)
{
    lua_newtable(L);

    int i = 1;
    struct cwc_seat *seat;
    wl_list_for_each(seat, &server.input->seats, link)
    {
        luaC_object_push(L, seat->cursor);
        lua_rawseti(L, -2, i++);
    }

    return 1;
}

/** Register a mouse binding.
 *
 * @staticfct bind
 * @tparam table|number modifier Table of modifier or modifier bitfield
 * @tparam number mouse_btn Button from linux input-event-codes
 * @tparam func on_press Function to execute when pressed
 * @tparam[opt] func on_release Function to execute when released
 * @tparam[opt] table data Additional data
 * @tparam[opt] string data.group Keybinding group
 * @tparam[opt] string data.description Keybinding description
 * @noreturn
 * @see cuteful.enum.modifier
 * @see cuteful.enum.mouse_btn
 * @see cwc.kbd.bind
 */
static int luaC_pointer_bind(lua_State *L)
{
    uint32_t button = luaL_checknumber(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    uint32_t modifiers = 0;
    if (lua_istable(L, 1)) {
        int len = lua_objlen(L, 1);

        for (int i = 0; i < len; ++i) {
            lua_rawgeti(L, 1, i + 1);
            modifiers |= luaL_checkint(L, -1);
        }

    } else if (lua_isnumber(L, 1)) {
        modifiers = lua_tonumber(L, 1);
    } else {
        luaL_error(L,
                   "modifiers only accept array of number or modifier bitmask");
    }

    bool on_press_is_function   = lua_isfunction(L, 3);
    bool on_release_is_function = lua_isfunction(L, 4);

    if (!on_press_is_function && !on_release_is_function) {
        luaL_error(L, "callback function is not provided");
        return 0;
    }

    struct cwc_keybind_info info = {0};
    info.type                    = CWC_KEYBIND_TYPE_LUA;

    if (on_press_is_function) {
        lua_pushvalue(L, 3);
        info.luaref_press = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    if (on_release_is_function) {
        lua_pushvalue(L, 4);
        info.luaref_release = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    keybind_mouse_register(server.main_mouse_kmap, modifiers, button, info);

    return 0;
}

/** Clear all mouse binding.
 *
 * @staticfct clear
 * @noreturn
 */
static int luaC_pointer_clear(lua_State *L)
{
    cwc_keybind_map_clear(server.main_mouse_kmap);
    return 0;
}

/** Get main seat pointer position.
 *
 * @staticfct get_position
 * @treturn table Pointer coords with structure {x,y}
 */
static int luaC_pointer_static_get_position(lua_State *L)
{
    double x = server.seat->cursor->wlr_cursor->x;
    double y = server.seat->cursor->wlr_cursor->y;

    lua_createtable(L, 0, 2);
    lua_pushnumber(L, x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, y);
    lua_setfield(L, -2, "y");

    return 1;
}

/** Set main seat pointer position.
 *
 * @staticfct set_position
 * @tparam integer x
 * @tparam integer y
 * @noreturn
 */
static int luaC_pointer_static_set_position(lua_State *L)
{
    int x = luaL_checkint(L, 1);
    int y = luaL_checkint(L, 2);

    wlr_cursor_warp(server.seat->cursor->wlr_cursor, NULL, x, y);

    return 1;
}

/** Start interactive move for client under the cursor.
 *
 * @staticfct move_interactive
 * @noreturn
 */
static int luaC_pointer_move_interactive(lua_State *L)
{
    start_interactive_move(NULL);
    return 0;
}

/** Start interactive resize for client under the cursor.
 *
 * @staticfct resize_interactive
 * @noreturn
 */
static int luaC_pointer_resize_interactive(lua_State *L)
{
    start_interactive_resize(NULL, 0);
    return 0;
}

/** Stop interactive mode.
 *
 * @staticfct stop_interactive
 * @noreturn
 */
static int luaC_pointer_stop_interactive(lua_State *L)
{
    stop_interactive(NULL);
    return 0;
}

/** Set cursor size.
 *
 * @tfield integer cursor_size
 */
static int luaC_pointer_get_cursor_size(lua_State *L)
{
    lua_pushnumber(L, g_config.cursor_size);
    return 1;
}
static int luaC_pointer_set_cursor_size(lua_State *L)
{
    int size = luaL_checkint(L, 1);

    g_config.cursor_size = size;
    return 0;
}

/** Set a timeout in seconds to automatically hide cursor, set timeout to 0 to
 * disable.
 *
 * @tfield integer inactive_timeout
 */
static int luaC_pointer_get_inactive_timeout(lua_State *L)
{
    lua_pushnumber(L, g_config.cursor_inactive_timeout);
    return 1;
}
static int luaC_pointer_set_inactive_timeout(lua_State *L)
{
    int seconds = luaL_checkint(L, 1);

    g_config.cursor_inactive_timeout = seconds * 1000;
    return 0;
}

/** Set a threshold distance for applying common tile position in pixel unit.
 *
 * @tfield integer edge_threshold
 */
static int luaC_pointer_get_edge_threshold(lua_State *L)
{
    lua_pushnumber(L, g_config.cursor_edge_threshold);
    return 1;
}
static int luaC_pointer_set_edge_threshold(lua_State *L)
{
    int threshold = luaL_checkint(L, 1);

    g_config.cursor_edge_threshold = threshold;
    return 0;
}

/** Set color of the overlay when performing edge snapping.
 *
 * `gears.color` is not gonna work because the overlay isn't a cairo surface.
 *
 * @configfct set_edge_snapping_overlay_color
 * @tparam number red Value of red.
 * @tparam number green Value of green.
 * @tparam number blue Value of blue.
 * @tparam number alpha Alpha value.
 * @noreturn
 */
static int luaC_pointer_set_edge_snapping_overlay_color(lua_State *L)
{
    float red   = luaL_checknumber(L, 1);
    float green = luaL_checknumber(L, 2);
    float blue  = luaL_checknumber(L, 3);
    float alpha = luaL_checknumber(L, 4);

    g_config.cursor_edge_snapping_overlay_color[0] = red;
    g_config.cursor_edge_snapping_overlay_color[1] = green;
    g_config.cursor_edge_snapping_overlay_color[2] = blue;
    g_config.cursor_edge_snapping_overlay_color[3] = alpha;
    return 0;
}

/** The seat names which the keyboard belong.
 *
 * @property seat
 * @tparam[opt="seat0"] string seat
 * @readonly
 */
static int luaC_pointer_get_seat(lua_State *L)
{
    struct cwc_cursor *cursor = luaC_pointer_checkudata(L, 1);
    lua_pushstring(L, cursor->seat->name);
    return 1;
}

/** The seat names which the keyboard belong.
 *
 * @property position
 * @tparam table position
 * @tparam table position.x The x coordinate of the pointer
 * @tparam table position.y The y coordinate of the pointer
 * @readonly
 */
static int luaC_pointer_get_position(lua_State *L)
{
    struct cwc_cursor *cursor = luaC_pointer_checkudata(L, 1);

    lua_createtable(L, 0, 2);
    lua_pushnumber(L, cursor->wlr_cursor->x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, cursor->wlr_cursor->y);
    lua_setfield(L, -2, "y");

    return 1;
}
static int luaC_pointer_set_position(lua_State *L)
{
    struct cwc_cursor *cursor = luaC_pointer_checkudata(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    lua_getfield(L, 2, "x");
    luaL_checktype(L, -1, LUA_TNUMBER);
    lua_getfield(L, 2, "y");
    luaL_checktype(L, -1, LUA_TNUMBER);

    wlr_cursor_warp(cursor->wlr_cursor, NULL, lua_tonumber(L, -2),
                    lua_tonumber(L, -1));

    return 0;
}

/** Grab the mouse event and redirect it to signal.
 *
 * @property grab
 * @tparam[opt=false] boolean grab
 * @readonly
 */
static int luaC_pointer_get_grab(lua_State *L)
{
    struct cwc_cursor *cursor = luaC_pointer_checkudata(L, 1);
    lua_pushboolean(L, cursor->grab);

    return 1;
}
static int luaC_pointer_set_grab(lua_State *L)
{
    struct cwc_cursor *cursor = luaC_pointer_checkudata(L, 1);
    bool grab                 = lua_toboolean(L, 2);
    cursor->grab              = grab;

    return 0;
}

/** Send pointer events to the client.
 *
 * @property send_events
 * @tparam[opt=true] boolean send_events
 * @readonly
 */
static int luaC_pointer_get_send_events(lua_State *L)
{
    struct cwc_cursor *cursor = luaC_pointer_checkudata(L, 1);
    lua_pushboolean(L, cursor->send_events);

    return 1;
}
static int luaC_pointer_set_send_events(lua_State *L)
{
    struct cwc_cursor *cursor = luaC_pointer_checkudata(L, 1);
    bool send_events          = lua_toboolean(L, 2);
    cursor->send_events       = send_events;

    return 0;
}

/** Move pointer relative the current position.
 *
 * @method move
 * @tparam integer x The x vector.
 * @tparam integer y The y vector.
 * @tparam[opt=false] boolean skip_events The motion won't be sent to client.
 * @noreturn
 */
static int luaC_pointer_move(lua_State *L)
{
    struct cwc_cursor *cursor = luaC_pointer_checkudata(L, 1);
    double x                  = luaL_checknumber(L, 2);
    double y                  = luaL_checknumber(L, 3);
    bool skip_events          = lua_toboolean(L, 4);

    if (skip_events) {
        wlr_cursor_move(cursor->wlr_cursor, NULL, x, y);
        return 0;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t now_msec = timespec_to_msec(&now);
    process_cursor_motion(cursor, now_msec, NULL, x, y, x, y);

    return 0;
}

/** Move pointer to the specified coordinate.
 *
 * @method move_to
 * @tparam integer x The new x position.
 * @tparam integer y The new y position.
 * @tparam[opt=false] boolean skip_events The motion won't be sent to client.
 * @noreturn
 * @noreturn
 */
static int luaC_pointer_move_to(lua_State *L)
{
    struct cwc_cursor *cursor = luaC_pointer_checkudata(L, 1);
    double x                  = luaL_checknumber(L, 2);
    double y                  = luaL_checknumber(L, 3);
    bool skip_events          = lua_toboolean(L, 4);

    double dx = x - cursor->wlr_cursor->x;
    double dy = y - cursor->wlr_cursor->y;

    if (skip_events) {
        wlr_cursor_move(cursor->wlr_cursor, NULL, x, y);
        return 0;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t now_msec = timespec_to_msec(&now);
    process_cursor_motion(cursor, now_msec, NULL, dx, dy, dx, dy);

    return 0;
}

#define REG_METHOD(name)    {#name, luaC_pointer_##name}
#define REG_READ_ONLY(name) {"get_" #name, luaC_pointer_get_##name}
#define REG_SETTER(name)    {"set_" #name, luaC_pointer_set_##name}
#define REG_PROPERTY(name)  REG_READ_ONLY(name), REG_SETTER(name)

#define FIELD_RO(name)     {"get_" #name, luaC_pointer_get_##name}
#define FIELD_SETTER(name) {"set_" #name, luaC_pointer_set_##name}
#define FIELD(name)        FIELD_RO(name), FIELD_SETTER(name)

void luaC_pointer_setup(lua_State *L)
{
    luaL_Reg pointer_metamethods[] = {
        {"__eq",       luaC_pointer_eq      },
        {"__tostring", luaC_pointer_tostring},
        {NULL,         NULL                 },
    };

    luaL_Reg pointer_methods[] = {
        REG_METHOD(move),       REG_METHOD(move_to),

        REG_READ_ONLY(data),    REG_READ_ONLY(seat),

        REG_PROPERTY(position), REG_PROPERTY(grab),  REG_PROPERTY(send_events),

        {NULL, NULL},
    };

    luaC_register_class(L, pointer_classname, pointer_methods,
                        pointer_metamethods);

    luaL_Reg pointer_staticlibs[] = {
        {"get",                             luaC_pointer_get                },

        {"bind",                            luaC_pointer_bind               },
        {"clear",                           luaC_pointer_clear              },

        {"get_position",                    luaC_pointer_static_get_position},
        {"set_position",                    luaC_pointer_static_set_position},

        {"move_interactive",                luaC_pointer_move_interactive   },
        {"resize_interactive",              luaC_pointer_resize_interactive },
        {"stop_interactive",                luaC_pointer_stop_interactive   },

        FIELD(cursor_size),
        FIELD(inactive_timeout),
        FIELD(edge_threshold),
        {"set_edge_snapping_overlay_color",
         luaC_pointer_set_edge_snapping_overlay_color                       },

        {NULL,                              NULL                            },
    };

    luaC_register_table(L, "cwc.pointer", pointer_staticlibs, NULL);
    lua_setfield(L, -2, "pointer");
}
