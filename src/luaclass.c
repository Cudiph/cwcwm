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

#include <lauxlib.h>
#include <lua.h>

const char *const client_classname      = "cwc_client";
const char *const container_classname   = "cwc_container";
const char *const screen_classname      = "cwc_screen";
const char *const tag_classname         = "cwc_tag";
const char *const input_classname       = "cwc_input";
const char *const layer_shell_classname = "cwc_layer_shell";

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
