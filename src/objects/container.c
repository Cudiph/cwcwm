/* container.c - lua cwc_container object
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

/** Low-level API client container operation.
 *
 * @author Dwi Asmoro Bangun
 * @copyright 2024
 * @license GPLv3
 * @coreclassmod cwc.container
 */

#include "cwc/layout/container.h"
#include <lauxlib.h>
#include <lua.h>

#include "cwc/desktop/toplevel.h"
#include "cwc/luac.h"
#include "cwc/luaclass.h"
#include "cwc/luaobject.h"
#include "cwc/server.h"

/** Emitted when a container is created.
 *
 * @signal container::new
 * @tparam cwc_container cont The container object.
 */

/** Emitted when a container is destroyed.
 *
 * @signal container::destroy
 * @tparam cwc_container cont The container object.
 */

/** Emitted when a client is inserted to the container.
 *
 * @signal container::insert
 * @tparam cwc_container cont The container object.
 * @tparam cwc_client client The inserted client.
 */

/** Emitted when a client is removed from the container.
 *
 * @signal container::remove
 * @tparam cwc_container cont The container object.
 * @tparam cwc_client client The removed client.
 */

/** Emitted when a container is swapped.
 *
 * @signal container::swap
 * @tparam cwc_container cont1 The container object.
 * @tparam cwc_container cont2 The container object.
 */

//=================== CODE ===========================

/** Navigate toplevel in the container.
 *
 * @method focusidx
 * @tparam integer idx Step relative to front client negative backward.
 * @noreturn
 */
static int luaC_container_focusidx(lua_State *L)
{
    struct cwc_container *container = luaC_container_checkudata(L, 1);
    int idx                         = luaL_checkint(L, 2);

    cwc_container_focusidx(container, idx);

    return 0;
}

/** Swap the toplevels inside the container.
 *
 * @method swap
 * @tparam cwc_container container Container to swap with this container.
 * @noreturn
 */
static int luaC_container_swap(lua_State *L)
{
    struct cwc_container *container  = luaC_container_checkudata(L, 1);
    struct cwc_container *container2 = luaC_container_checkudata(L, 2);

    cwc_container_swap(container, container2);

    return 0;
}

/** Insert client to this container.
 *
 * @method insert_client
 * @tparam cwc_client client Client to insert.
 * @noreturn
 */
static int luaC_container_insert_client(lua_State *L)
{
    struct cwc_container *container = luaC_container_checkudata(L, 1);
    struct cwc_toplevel *toplevel   = luaC_client_checkudata(L, 2);

    if (container == toplevel->container)
        return 0;

    if (toplevel->container)
        cwc_container_remove_toplevel(toplevel);

    cwc_container_insert_toplevel(container, toplevel);

    return 0;
}

static void _push_clientstack(struct cwc_toplevel *toplevel, void *data)
{
    lua_State *L = data;
    int idx      = luaL_checkint(L, -1);
    luaC_object_push(L, toplevel);
    lua_rawseti(L, -3, idx);
    lua_pop(L, 1);
    lua_pushinteger(L, ++idx);
}

/** Get clients in the container ordered by the position in the scene.
 *
 * @method get_client_stack
 * @tparam boolean reverse If true the order is bottom to front.
 * @treturn cwc_client[] clients Array of clients.
 * @propertydefault nil if empty.
 */
static int luaC_container_get_client_stack(lua_State *L)
{
    struct cwc_container *container = luaC_container_checkudata(L, 1);
    bool reverse                    = lua_toboolean(L, 2);

    lua_newtable(L);
    lua_pushnumber(L, 1);

    if (reverse)
        cwc_container_for_each_bottom_to_top(container, _push_clientstack, L);
    else
        cwc_container_for_each_toplevel_top_to_bottom(container,
                                                      _push_clientstack, L);

    lua_pop(L, 1);

    return 1;
}

/** Get clients in the container.
 *
 * Ordered by time the toplevel inserted (first item is newest to oldest).
 *
 * @property clients
 * @readonly
 * @tparam cwc_client[] clients Array of clients.
 * @propertydefault nil if empty.
 */
static int luaC_container_get_clients(lua_State *L)
{
    struct cwc_container *container = luaC_container_checkudata(L, 1);

    lua_newtable(L);

    struct cwc_toplevel *toplevel;
    int i = 1;
    wl_list_for_each(toplevel, &container->toplevels, link_container)
    {
        luaC_object_push(L, toplevel);
        lua_rawseti(L, -2, i++);
    }

    return 1;
}

/** Get the topmost client.
 *
 * @property front
 * @tparam cwc_client front The client.
 * @readonly
 * @propertydefault client in the top of the view.
 */
static int luaC_container_get_front(lua_State *L)
{
    struct cwc_container *container = luaC_container_checkudata(L, 1);
    struct cwc_toplevel *top = cwc_container_get_front_toplevel(container);

    return luaC_object_push(L, top);
}

/** Geometry of the container in global coordinate.
 *
 * @property geometry
 * @tparam table geometry
 * @tparam integer geometry.x
 * @tparam integer geometry.y
 * @tparam integer geometry.width
 * @tparam integer geometry.height
 * @propertydefault current container geometry with structure box structure.
 */
static int luaC_container_get_geometry(lua_State *L)
{
    struct cwc_container *container = luaC_container_checkudata(L, 1);

    struct wlr_box geom = cwc_container_get_box(container);

    return luaC_pushbox(L, geom);
}

static int luaC_container_set_geometry(lua_State *L)
{
    luaL_checktype(L, 2, LUA_TTABLE);

    struct cwc_container *container = luaC_container_checkudata(L, 1);
    struct wlr_box box              = cwc_container_get_box(container);

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

    cwc_container_set_box_global(container, &box);

    return 1;
}

/** Mark container so that the next mapped toplevel would be inserted to it.
 *
 * @property insert_mark
 * @tparam[opt=false] boolean insert_mark Whether the container has insert mark.
 * @see reset_mark
 */
static int luaC_container_get_insert_mark(lua_State *L)
{
    struct cwc_container *container = luaC_container_checkudata(L, 1);

    lua_pushboolean(L, container == server.insert_marked);

    return 1;
}

static int luaC_container_set_insert_mark(lua_State *L)
{
    struct cwc_container *container = luaC_container_checkudata(L, 1);

    server.insert_marked = container;

    return 0;
}

/** Get all containers in the server.
 *
 * @staticfct get
 * @treturn cwc_container[]
 */
static int luaC_container_get(lua_State *L)
{
    lua_newtable(L);

    struct cwc_container *container;
    int i = 1;
    wl_list_for_each(container, &server.containers, link)
    {
        luaC_object_push(L, container);
        lua_rawseti(L, -2, i++);
    }

    return 1;
}

/** Reset mark.
 *
 * @staticfct reset_mark
 * @noreturn
 */
static int luaC_container_reset_mark(lua_State *L)
{
    server.insert_marked = NULL;

    return 0;
}

#define REG_READ_ONLY(name) {"get_" #name, luaC_container_get_##name}
#define REG_SETTER(name)    {"set_" #name, luaC_container_set_##name}
#define REG_PROPERTY(name)  REG_READ_ONLY(name), REG_SETTER(name)

void luaC_container_setup(lua_State *L)
{
    luaL_Reg container_metamethods[] = {
        {"__eq",       luaC_container_eq      },
        {"__tostring", luaC_container_tostring},
        {NULL,         NULL                   },
    };

    luaL_Reg container_methods[] = {
        {"focusidx",         luaC_container_focusidx        },
        {"swap",             luaC_container_swap            },
        {"insert_client",    luaC_container_insert_client   },

        // ro props but argument available
        {"get_client_stack", luaC_container_get_client_stack},

        // readonly
        REG_READ_ONLY(data),
        REG_READ_ONLY(clients),
        REG_READ_ONLY(front),

        // properties
        REG_PROPERTY(geometry),
        REG_PROPERTY(insert_mark),

        {NULL,               NULL                           },
    };

    luaC_register_class(L, container_classname, container_methods,
                        container_metamethods);

    luaL_Reg container_staticlibs[] = {
        {"get",        luaC_container_get       },
        {"reset_mark", luaC_container_reset_mark},
        {NULL,         NULL                     },
    };

    luaC_register_table(L, "cwc.container", container_staticlibs, NULL);
    lua_setfield(L, -2, "container");
}
