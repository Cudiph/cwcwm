/* luaclass.c - lua classified object management
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

#include "cwc/luac.h"
#include <lauxlib.h>
#include <lua.h>

const char *const client_classname      = "cwc_client";
const char *const container_classname   = "cwc_container";
const char *const screen_classname      = "cwc_screen";
const char *const tag_classname         = "cwc_tag";
const char *const input_classname       = "cwc_input";
const char *const layer_shell_classname = "cwc_layer_shell";
const char *const kbindmap_classname    = "cwc_kbindmap";
const char *const kbind_classname       = "cwc_kbind";
const char *const timer_classname       = "cwc_timer";

/* equivalent lua code:
 * function(t, k)
 *
 *   local mt = getmetatable(t)
 *   local index = mt.__cwcindex
 *
 *   if index["get_" .. k] then return index["get_" .. k]() end
 *
 *   return index[k]
 *
 * end
 */
static int luaC_getter(lua_State *L)
{
    if (!lua_getmetatable(L, 1))
        return 0;

    lua_getfield(L, -1, "__cwcindex");

    lua_pushstring(L, "get_");
    lua_pushvalue(L, 2);
    lua_concat(L, 2);

    lua_rawget(L, -2);

    if (lua_isfunction(L, -1)) {
        lua_pushvalue(L, 1);
        lua_call(L, 1, 1);
        return 1;
    }

    lua_pop(L, 1);
    lua_pushvalue(L, 2);
    lua_gettable(L, -2);

    return 1;
}

/* equivalent lua code:
 * function(t, k, v)
 *
 *   local mt = getmetatable(t)
 *   local index = mt.__cwcindex
 *
 *   if not index["set_" .. k] then return end
 *
 *   mt.[k](t, v)
 *
 * end
 */
static int luaC_setter(lua_State *L)
{
    if (!lua_getmetatable(L, 1))
        return 0;

    lua_getfield(L, -1, "__cwcindex");

    lua_pushstring(L, "set_");
    lua_pushvalue(L, 2);
    lua_concat(L, 2);

    lua_rawget(L, -2);

    if (lua_isnil(L, -1))
        return 0;

    lua_pushvalue(L, 1);
    lua_pushvalue(L, 3);

    lua_call(L, 2, 0);

    return 1;
}

/* methods that start with `get_` can be accessed without the prefix,
 * for example c:get_fullscreen() is the same as c.fullscreen
 *
 * [-0, +0, -]
 */
void luaC_register_class(lua_State *L,
                         const char *classname,
                         luaL_Reg methods[],
                         luaL_Reg metamethods[])
{
    // create the metatable and register the metamethods other than
    // index and newindex
    luaL_newmetatable(L, classname);
    luaL_register(L, NULL, metamethods);

    lua_newtable(L);
    luaL_register(L, NULL, methods);
    lua_setfield(L, -2, "__cwcindex");

    lua_pushcfunction(L, luaC_getter);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, luaC_setter);
    lua_setfield(L, -2, "__newindex");

    // pop metatable
    lua_pop(L, 1);
}

/* equivalent lua code:
 * function(t, k)
 *
 *   local mt = getmetatable(t)
 *   local index = mt.__cwcindex
 *
 *   if index["get_" .. k] then return index["get_" .. k]() end
 *   if index[k] then return index[k] end
 *
 *   -- ====================================================
 *
 *   if t["get_" .. k] then return t["get_" .. k]() end
 *
 *   return t[k]
 *
 * end
 */
static int luaC_table_getter(lua_State *L)
{
    if (!lua_getmetatable(L, 1))
        return 0;

    lua_getfield(L, -1, "__cwcindex");

    lua_pushstring(L, "get_");
    lua_pushvalue(L, 2);
    lua_concat(L, 2);

    lua_rawget(L, -2);

    if (lua_isfunction(L, -1)) {
        lua_pushvalue(L, 1);
        lua_call(L, 1, 1);
        return 1;
    }

    lua_pop(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    if (!lua_isnil(L, -1))
        return 1;

    // ====================================================

    lua_settop(L, 2);
    lua_pushvalue(L, 1);

    lua_pushstring(L, "get_");
    lua_pushvalue(L, 2);
    lua_concat(L, 2);

    lua_rawget(L, 1);

    if (lua_isfunction(L, -1)) {
        lua_pushvalue(L, 1);
        lua_call(L, 1, 1);
        return 1;
    }

    lua_settop(L, 2);
    lua_rawget(L, 1);

    return 1;
}

/* equivalent lua code:
 * function(t, k, v)
 *
 *   local mt = getmetatable(t)
 *   local index = mt.__cwcindex
 *
 *   if index["set_" .. k] then return index["set_" .. k](v) end
 *   if t["set_" .. k] then return t["set_" .. k](v) end
 *
 *   if not index["get_" .. k] and not index["set_" .. k] then
 *     t[k] = v
 *   end
 *
 * end
 */
static int luaC_table_setter(lua_State *L)
{
    if (!lua_getmetatable(L, 1))
        return 0;

    lua_getfield(L, -1, "__cwcindex");

    lua_pushstring(L, "set_");
    lua_pushvalue(L, 2);
    lua_concat(L, 2);

    lua_rawget(L, -2);

    if (lua_isfunction(L, -1)) {
        lua_pushvalue(L, 3);
        lua_call(L, 1, 0);
        return 0;
    }

    lua_pop(L, 1);
    lua_pushvalue(L, 1);

    lua_pushstring(L, "set_");
    lua_pushvalue(L, 2);
    lua_concat(L, 2);

    lua_rawget(L, -2);

    if (lua_isfunction(L, -1)) {
        lua_pushvalue(L, 3);
        lua_call(L, 1, 0);
        return 0;
    }

    lua_pop(L, 2);

    lua_pushstring(L, "set_");
    lua_pushvalue(L, 2);
    lua_concat(L, 2);

    lua_rawget(L, -2);

    lua_pushstring(L, "get_");
    lua_pushvalue(L, 2);
    lua_concat(L, 2);

    lua_rawget(L, -3);

    if (lua_isnil(L, -1) && lua_isnil(L, -2)) {
        lua_settop(L, 3);
        lua_rawset(L, 1);
    }

    return 0;
}

/* create/register table with getter and setter
 *
 * [-0, +1, -]
 */
void luaC_register_table(lua_State *L,
                         const char *classname,
                         luaL_Reg methods[],
                         luaL_Reg metamethods[])
{
    // create the metatable and register the metamethods other than
    // index and newindex
    luaL_newmetatable(L, classname);
    luaL_register(L, NULL, metamethods);

    lua_newtable(L);
    luaL_register(L, NULL, methods);
    lua_setfield(L, -2, "__cwcindex");

    lua_pushcfunction(L, luaC_table_getter);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, luaC_table_setter);
    lua_setfield(L, -2, "__newindex");

    lua_pop(L, 1);
    lua_newtable(L);
    luaL_getmetatable(L, classname);
    lua_setmetatable(L, -2);
}
