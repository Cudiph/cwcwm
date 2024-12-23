/* client.c - lua cwc_client object
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

/** Low-level API to manage toplevel/window/client.
 *
 * If a client is in container with more than one client only topfront client
 * that will emit property signal.
 *
 * See also: `cuteful.client`
 *
 * @author Dwi Asmoro Bangun
 * @copyright 2024
 * @license GPLv3
 * @coreclassmod cwc.client
 */

#include <cairo.h>
#include <lauxlib.h>
#include <lua.h>
#include <string.h>
#include <wayland-util.h>

#include "cwc/config.h"
#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/layout/bsp.h"
#include "cwc/layout/container.h"
#include "cwc/luac.h"
#include "cwc/luaclass.h"
#include "cwc/luaobject.h"
#include "cwc/server.h"

/** Emitted when a client is created.
 *
 * Client operation such as moving, resizing, etc. is not yet possible at this
 * stage and will trigger segfault, only read only property may be available.
 * If you want to do client transformation with the client wait for `map` signal
 * to appear.
 *
 * @signal client::new
 * @tparam cwc_client c The client object.
 * @see map
 */

/** Emitted when a client is about to be destroyed.
 *
 * @signal client::destroy
 * @tparam cwc_client c The client object.
 */

/** Emitted when a client is mapped to the screen.
 *
 * @signal client::map
 * @tparam cwc_client c The client object.
 */

/** Emitted when a client is about to be unmapped from the screen.
 *
 * @signal client::unmap
 * @tparam cwc_client c The client object.
 */

/** Emitted when a client gains focus.
 *
 * Unmanaged client won't emit.
 *
 * @signal client::focus
 * @tparam cwc_client c The client object.
 */

/** Emitted when a client lost focus.
 *
 * Unmanaged client won't emit.
 *
 * @signal client::unfocus
 * @tparam cwc_client c The client object.
 */

/** Emitted when a client is swapped.
 *
 * @signal client::swap
 * @tparam cwc_client c1 The client object.
 * @tparam cwc_client c2 The client object.
 */

/** Emitted when the mouse enters a client.
 *
 * @signal client::mouse_enter
 * @tparam cwc_client c The client object.
 */

/** Emitted when the mouse leaves a client.
 *
 * @signal client::mouse_leave
 * @tparam cwc_client c The client object.
 */

/** Emitted when the client is raised within its layer.
 *
 * @signal client::raised
 * @tparam cwc_client c The client object.
 */

/** Emitted when the client is lowered within its layer.
 *
 * @signal client::lowered
 * @tparam cwc_client c The client object.
 */

//================= PROPERTIES ===========================

/* just to keep track whats already done */

/** Property signal.
 *
 * @signal client::property::fullscreen
 * @tparam cwc_client c The client object.
 */

/** Property signal.
 *
 * @signal client::property::maximized
 * @tparam cwc_client c The client object.
 */

/** Property signal.
 *
 * @signal client::property::minimized
 * @tparam cwc_client c The client object.
 */

/** Property signal.
 *
 * @signal client::property::floating
 * @tparam cwc_client c The client object.
 */

//============================ CODE =================================

/** Resize client relative to current size.
 *
 * @method resize
 * @tparam integer width Current width + specified width
 * @tparam integer height Current height + specified height
 * @noreturn
 */
static int luaC_client_resize(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);
    int w                         = luaL_checkint(L, 2);
    int h                         = luaL_checkint(L, 3);

    struct wlr_box geom = cwc_toplevel_get_geometry(toplevel);
    cwc_toplevel_set_size_surface(toplevel, geom.width + w, geom.height + h);

    return 0;
}

/** Resize client to specified size.
 *
 * @method resize_to
 * @tparam number width Desired width
 * @tparam number width Desired height
 * @noreturn
 */
static int luaC_client_resize_to(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);
    int w                         = luaL_checkint(L, 2);
    int h                         = luaL_checkint(L, 3);

    cwc_toplevel_set_size(toplevel, w, h);

    return 0;
}

/** Move client relative to current position.
 *
 * @method move
 * @tparam integer x X coordinate
 * @tparam integer y Y coordinate
 * @noreturn
 */
static int luaC_client_move(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    int x = luaL_checkint(L, 2);
    int y = luaL_checkint(L, 3);

    struct wlr_box box = cwc_toplevel_get_box(toplevel);
    cwc_toplevel_set_position(toplevel, box.x + x, box.y + y);

    return 0;
}

/** Move client to the x y coordinates.
 *
 * @method move_to
 * @tparam integer x X coordinate
 * @tparam integer y Y coordinate
 * @noreturn
 */
static int luaC_client_move_to(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    int x = luaL_checkint(L, 2);
    int y = luaL_checkint(L, 3);

    cwc_toplevel_set_position(toplevel, x, y);

    return 0;
}

/** Get all clients into a table.
 *
 * @tparam[opt] cwc_screen screen A screen to filter clients on.
 * @tparam[opt=true] boolean skip_unmanaged Skip unmanaged client (xwayland
 * popup etc.).
 * @staticfct get
 * @treturn cwc_client[]
 */
static int luaC_client_get(lua_State *L)
{
    struct cwc_output *screen = NULL;

    if (lua_type(L, 1) == LUA_TUSERDATA)
        screen = luaC_screen_checkudata(L, 1);

    int skip_unmanaged = lua_toboolean(L, 2);

    lua_newtable(L);

    struct cwc_toplevel *toplevel;
    int i = 1;
    wl_list_for_each(toplevel, &server.toplevels, link)
    {
        if (skip_unmanaged && cwc_toplevel_is_unmanaged(toplevel))
            continue;

        if (screen && toplevel->container->output != screen)
            continue;

        luaC_object_push(L, toplevel);
        lua_rawseti(L, -2, i++);
    }

    return 1;
}

/** Get client at x y coordinates.
 *
 * @staticfct at
 * @tparam integer x X coordinate
 * @tparam integer y Y coordinate
 * @treturn cwc_client
 */
static int luaC_client_at(lua_State *L)
{
    double x = lua_tonumber(L, 1);
    double y = lua_tonumber(L, 2);

    double sx, sy;
    struct cwc_toplevel *toplevel = cwc_toplevel_at(x, y, &sx, &sy);
    if (toplevel)
        luaC_object_push(L, toplevel);
    else
        lua_pushnil(L);

    return 1;
}

/** Get currently focused client.
 *
 * @staticfct focused
 * @treturn cwc_client
 */
static int luaC_client_focused(lua_State *L)
{
    struct cwc_toplevel *toplevel = cwc_toplevel_get_focused();
    if (toplevel)
        luaC_object_push(L, toplevel);
    else
        lua_pushnil(L);

    return 1;
}

/** Set the default border thickness.
 *
 * @configfct set_border_width
 * @tparam integer width Width in pixels
 * @noreturn
 */
static int luaC_client_set_border_width(lua_State *L)
{
    int bw = luaL_checkint(L, 1);

    cwc_config_set_number_positive(&g_config.border_width, bw);

    return 0;
}

/** Set the default border color for focused client.
 *
 * @configfct set_border_color_focus
 * @tparam pattern cairo_pattern_t Cairo pattern object from gears.color
 * @noreturn
 * @see gears.color
 */
static int luaC_client_set_border_color_focus(lua_State *L)
{
    cairo_pattern_t *pattern = luaC_checkcolor(L, 1);
    cwc_config_set_cairo_pattern(&g_config.border_color_focus, pattern);

    return 0;
}

/** Set the default border color for toplevel.
 *
 * @configfct set_border_color_normal
 * @tparam pattern cairo_pattern_t Cairo pattern object from gears.color
 * @noreturn
 * @see gears.color
 */
static int luaC_client_set_border_color_normal(lua_State *L)
{
    cairo_pattern_t *pattern = luaC_checkcolor(L, 1);
    cwc_config_set_cairo_pattern(&g_config.border_color_normal, pattern);

    return 0;
}

/** Set the border color rotation config in degree
 *
 * @configfct set_border_color_rotation
 * @tparam integer degree Rotation in degree
 * @noreturn
 */
static int luaC_client_set_border_color_rotation(lua_State *L)
{
    int rot = luaL_checkint(L, 1);

    g_config.border_color_rotation_degree = rot;

    return 0;
}

/** Request to close a client.
 *
 * @method close
 * @noreturn
 */
static int luaC_client_close(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    cwc_toplevel_send_close(toplevel);

    return 0;
}

/** Kill a client without any question asked.
 *
 * @method kill
 * @noreturn
 */
static int luaC_client_kill(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    cwc_toplevel_kill(toplevel);

    return 0;
}

/** Raise toplevel to top of other toplevels.
 *
 * @method raise
 * @noreturn
 */
static int luaC_client_raise(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    cwc_container_raise(toplevel->container);

    return 0;
}

/** Lower toplevel to bottom of other toplevels.
 *
 * @method lower
 * @noreturn
 */
static int luaC_client_lower(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    cwc_container_lower(toplevel->container);

    return 0;
}

/** Set a keyboard focus for a client.
 *
 * @method focus
 * @noreturn
 */
static int luaC_client_focus(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    cwc_toplevel_focus(toplevel, false);

    return 0;
}

/** Swap client with another client.
 *
 * @method swap
 * @noreturn
 */
static int luaC_client_swap(lua_State *L)
{
    struct cwc_toplevel *toplevel  = luaC_client_checkudata(L, 1);
    struct cwc_toplevel *toplevel2 = luaC_client_checkudata(L, 2);

    cwc_toplevel_swap(toplevel, toplevel2);

    return 0;
}

/** Centerize the client according to the screen workarea.
 *
 * @method center
 * @noreturn
 */
static int luaC_client_center(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    cwc_toplevel_to_center(toplevel);

    return 0;
}

#define CLIENT_PROPERTY_CREATE_BOOLEAN_RO(name)                       \
    static int luaC_client_get_##name(lua_State *L)                   \
    {                                                                 \
        struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1); \
        lua_pushboolean(L, cwc_toplevel_is_##name(toplevel));         \
        return 1;                                                     \
    }

#define CLIENT_PROPERTY_CREATE_BOOLEAN(name)                          \
    CLIENT_PROPERTY_CREATE_BOOLEAN_RO(name)                           \
                                                                      \
    static int luaC_client_set_##name(lua_State *L)                   \
    {                                                                 \
        struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1); \
        bool set                      = lua_toboolean(L, 2);          \
        cwc_toplevel_set_##name(toplevel, set);                       \
        return 0;                                                     \
    }

/**
 * The client is mapped on the screen or not.
 *
 * @property mapped
 * @readonly
 * @tparam boolean mapped
 * @propertydefault Depends on the client state.
 */
CLIENT_PROPERTY_CREATE_BOOLEAN_RO(mapped)

/**
 * The client is visible/rendered on the screen or not.
 *
 * @property visible
 * @readonly
 * @tparam[opt=true] boolean visible
 */
CLIENT_PROPERTY_CREATE_BOOLEAN_RO(visible)

/**
 * The client is xwayland or not.
 *
 * @property x11
 * @readonly
 * @tparam boolean x11
 * @propertydefault Depends on the client.
 */
CLIENT_PROPERTY_CREATE_BOOLEAN_RO(x11)

/**
 * The client is unmanaged or not.
 *
 * @property unmanaged
 * @readonly
 * @tparam boolean unmanaged
 * @propertydefault Depends on the client.
 */
CLIENT_PROPERTY_CREATE_BOOLEAN_RO(unmanaged)

/**
 * The client fullscreen state.
 *
 * @property fullscreen
 * @tparam[opt=false] boolean fullscreen
 * @see maximized
 */
CLIENT_PROPERTY_CREATE_BOOLEAN(fullscreen)

/**
 * The client maximized state.
 *
 * @property maximized
 * @tparam[opt=false] boolean maximized
 * @see fullscreen
 */
CLIENT_PROPERTY_CREATE_BOOLEAN(maximized)

/** The client floating state.
 *
 * Always return true if the layout mode is floating.
 *
 * @property floating
 * @tparam[opt=false] boolean floating
 */
CLIENT_PROPERTY_CREATE_BOOLEAN(floating)

/** The client minimized state.
 *
 * @property minimized
 * @tparam[opt=false] boolean minimized
 */
CLIENT_PROPERTY_CREATE_BOOLEAN(minimized)

/** Set the client sticky (Available on all tags).
 *
 * @property sticky
 * @tparam[opt=false] boolean sticky
 */
CLIENT_PROPERTY_CREATE_BOOLEAN(sticky)

/** The client is on top of every other window.
 *
 * @property ontop
 * @tparam[opt=false] boolean ontop
 */
CLIENT_PROPERTY_CREATE_BOOLEAN(ontop)

/** The client is above normal window.
 *
 * @property above
 * @tparam[opt=false] boolean above
 */
CLIENT_PROPERTY_CREATE_BOOLEAN(above)

/** The client is below normal window.
 *
 * @property below
 * @tparam[opt=false] boolean below
 */
CLIENT_PROPERTY_CREATE_BOOLEAN(below)

/** Allow the client to tear when in fullscreen.
 *
 * @property allow_tearing
 * @tparam[opt=false] boolean allow_tearing
 * @see cwc.screen.allow_tearing
 */
CLIENT_PROPERTY_CREATE_BOOLEAN(allow_tearing)

/** Geometry of the client (border not included).
 *
 * @property geometry
 * @tparam table geometry
 * @tparam integer geometry.x
 * @tparam integer geometry.y
 * @tparam integer geometry.width
 * @tparam integer geometry.height
 * @propertydefault current client geometry with structure {x,y,width,height}.
 * @see cwc.container.geometry
 */
static int luaC_client_get_geometry(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    struct wlr_box box = cwc_toplevel_get_box(toplevel);

    return luaC_pushbox(L, box);
}

static int luaC_client_set_geometry(lua_State *L)
{
    luaL_checktype(L, 2, LUA_TTABLE);

    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);
    struct wlr_box box            = cwc_toplevel_get_box(toplevel);

    lua_getfield(L, 2, "x");
    if (!lua_isnil(L, -1))
        box.x = luaL_checkint(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "y");
    if (!lua_isnil(L, -1))
        box.y = luaL_checkint(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "width");
    if (!lua_isnil(L, -1))
        box.width = luaL_checkint(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "height");
    if (!lua_isnil(L, -1))
        box.height = luaL_checkint(L, -1);
    lua_pop(L, 1);

    cwc_toplevel_set_position(toplevel, box.x, box.y);
    cwc_toplevel_set_size(toplevel, box.width, box.height);

    return 0;
}

/** The client tag.
 *
 * @property tag
 * @tparam integer tag
 * @negativeallowed false
 * @propertydefault the active tag when the client is mapped.
 */
static int luaC_client_get_tag(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    lua_pushinteger(L, toplevel->container->tag);

    return 1;
}

static int luaC_client_set_tag(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);
    tag_bitfield_t tag            = luaL_checkint(L, 2);

    toplevel->container->tag = tag;
    cwc_output_update_visible(toplevel->container->output);

    return 0;
}

/** The client workspace.
 *
 * @property workspace
 * @tparam integer workspace
 * @negativeallowed false
 * @propertydefault the active workspace when the client is mapped.
 */
static int luaC_client_get_workspace(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    lua_pushinteger(L, toplevel->container->workspace);

    return 1;
}

/** Move client to specified tag index.
 *
 * Equivalent as setting the workspace property.
 *
 * @method move_to_tag
 * @tparam integer tag Tag index
 * @noreturn
 * @see workspace
 */
static int luaC_client_set_workspace(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    int view = luaL_checkint(L, 2);
    cwc_toplevel_move_to_tag(toplevel, view);

    return 0;
}

/** The client opacity.
 *
 * @property opacity
 * @rangestart 0.0
 * @rangestop 1.0
 * @tparam[opt=1.0] number opacity
 */
static int luaC_client_get_opacity(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    lua_pushnumber(L, cwc_toplevel_get_opacity(toplevel));

    return 1;
}

static int luaC_client_set_opacity(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    float opacity = luaL_checknumber(L, 2);
    cwc_toplevel_set_opacity(toplevel, opacity);

    return 0;
}

/** The client parent.
 *
 * @property parent
 * @tparam cwc_client parent
 * @readonly
 * @propertydefault `nil`.
 */
static int luaC_client_get_parent(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    struct cwc_toplevel *parent = cwc_toplevel_get_parent(toplevel);

    if (parent)
        luaC_object_push(L, parent);
    else
        lua_pushnil(L);

    return 1;
}

/** The screen where the client stay.
 *
 * @property screen
 * @tparam cwc_screen screen
 * @readonly
 * @propertydefault The screen where the top left corner client placed.
 */
static int luaC_client_get_screen(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    luaC_object_push(L, toplevel->container->output);

    return 1;
}

/** The client pid.
 *
 * @property pid
 * @tparam string pid
 * @readonly
 * @propertydefault This is provided by the application.
 */
static int luaC_client_get_pid(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    lua_pushnumber(L, cwc_toplevel_get_pid(toplevel));

    return 1;
}

/** The client title.
 *
 * @property title
 * @tparam string title
 * @readonly
 * @propertydefault This is provided by the application.
 */
static int luaC_client_get_title(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    lua_pushstring(L, cwc_toplevel_get_title(toplevel));

    return 1;
}

/** The client app_id.
 *
 * @property appid
 * @tparam string appid
 * @readonly
 * @propertydefault This is provided by the application.
 */
static int luaC_client_get_appid(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    lua_pushstring(L, cwc_toplevel_get_app_id(toplevel));

    return 1;
}

/** The client container.
 *
 * @property container
 * @tparam cwc_container container
 * @readonly
 * @propertydefault the container that client attached to.
 */
static int luaC_client_get_container(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    return luaC_object_push(L, toplevel->container);
}

/** Get nearest client relative to this client.
 *
 * @method get_nearest
 * @tparam integer enum Direction
 * @treturn cwc_client
 * @see cuteful.enum.direction
 */
static int luaC_client_get_nearest(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);
    int direction                 = luaL_checkinteger(L, 2);

    luaC_object_push(
        L, cwc_toplevel_get_nearest_by_direction(toplevel, direction));

    return 1;
}

/** Toggle vsplit to hsplit or otherwise for bsp layout.
 *
 * @method toggle_split
 * @noreturn
 */
static int luaC_client_toggle_split(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    bsp_toggle_split(toplevel->container->bsp_node);

    return 0;
}

/** Toggle enable or disable a tag.
 *
 * @method toggle_tag
 * @tparam integer idx Tag position
 * @noreturn
 */
static int luaC_client_toggle_tag(lua_State *L)
{
    struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);

    int tag = luaL_checkint(L, 2);
    toplevel->container->tag ^= 1 << (tag - 1);
    cwc_output_update_visible(toplevel->container->output);

    return 0;
}

#define CLIENT_METHOD(name)        {#name, luaC_client_##name}
#define CLIENT_REG_READ_ONLY(name) {"get_" #name, luaC_client_get_##name}
#define CLIENT_REG_SETTER(name)    {"set_" #name, luaC_client_set_##name}
#define CLIENT_REG_PROPERTY(name) \
    CLIENT_REG_READ_ONLY(name), CLIENT_REG_SETTER(name)

void luaC_client_setup(lua_State *L)
{
    luaL_Reg client_metamethods[] = {
        {"__eq",       luaC_client_eq      },
        {"__tostring", luaC_client_tostring},
        {NULL,         NULL                },
    };

    luaL_Reg client_methods[] = {
        CLIENT_METHOD(move),
        CLIENT_METHOD(move_to),
        CLIENT_METHOD(resize),
        CLIENT_METHOD(resize_to),
        CLIENT_METHOD(close),
        CLIENT_METHOD(kill),
        CLIENT_METHOD(raise),
        CLIENT_METHOD(lower),
        CLIENT_METHOD(focus),
        CLIENT_METHOD(swap),
        CLIENT_METHOD(center),

        CLIENT_METHOD(toggle_split),
        CLIENT_METHOD(toggle_tag),

        {"move_to_tag", luaC_client_set_workspace},
        {"get_nearest", luaC_client_get_nearest  },

        // read only properties
        CLIENT_REG_READ_ONLY(pid),
        CLIENT_REG_READ_ONLY(title),
        CLIENT_REG_READ_ONLY(appid),
        CLIENT_REG_READ_ONLY(screen),
        CLIENT_REG_READ_ONLY(parent),
        CLIENT_REG_READ_ONLY(mapped),
        CLIENT_REG_READ_ONLY(visible),
        CLIENT_REG_READ_ONLY(x11),
        CLIENT_REG_READ_ONLY(unmanaged),
        CLIENT_REG_READ_ONLY(container),

        // properties
        CLIENT_REG_PROPERTY(geometry),
        CLIENT_REG_PROPERTY(tag),
        CLIENT_REG_PROPERTY(workspace),
        CLIENT_REG_PROPERTY(fullscreen),
        CLIENT_REG_PROPERTY(maximized),
        CLIENT_REG_PROPERTY(floating),
        CLIENT_REG_PROPERTY(minimized),
        CLIENT_REG_PROPERTY(sticky),
        CLIENT_REG_PROPERTY(ontop),
        CLIENT_REG_PROPERTY(above),
        CLIENT_REG_PROPERTY(below),
        CLIENT_REG_PROPERTY(opacity),
        CLIENT_REG_PROPERTY(allow_tearing),

        {NULL,          NULL                     },
    };

    luaC_register_class(L, client_classname, client_methods,
                        client_metamethods);

    luaL_Reg client_staticlibs[] = {
        {"get",                       luaC_client_get                      },
        {"at",                        luaC_client_at                       },
        {"focused",                   luaC_client_focused                  },

        {"set_border_width",          luaC_client_set_border_width         },
        {"set_border_color_focus",    luaC_client_set_border_color_focus   },
        {"set_border_color_normal",   luaC_client_set_border_color_normal  },
        {"set_border_color_rotation", luaC_client_set_border_color_rotation},
        {NULL,                        NULL                                 },
    };

    lua_newtable(L);
    luaL_register(L, NULL, client_staticlibs);
    lua_setfield(L, -2, "client");
}
