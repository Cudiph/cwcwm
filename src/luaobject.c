/* luaobject.c - lua object management
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

#include "cwc/luaobject.h"

const char *const LUAC_OBJECT_REGISTRY_KEY       = "cwc.object.registry";
const char *const LUAC_OBJECT_UDATA_REGISTRY_KEY = "cwc.object.data.registry";

/** Setup the object system at startup.
 * \param L The Lua VM state.
 */
void luaC_object_setup(lua_State *L)
{
    lua_pushstring(L, LUAC_OBJECT_REGISTRY_KEY);
    lua_newtable(L);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushstring(L, LUAC_OBJECT_UDATA_REGISTRY_KEY);
    lua_newtable(L);
    lua_rawset(L, LUA_REGISTRYINDEX);
}
