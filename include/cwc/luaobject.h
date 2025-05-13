/*
 * luaobject.h - useful functions for handling Lua objects
 *
 * Copyright © 2009 Julien Danjou <julien@danjou.info>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef _CWC_LUAOBJECT_H
#define _CWC_LUAOBJECT_H

#include <lauxlib.h>
#include <lua.h>

extern const char *const LUAC_OBJECT_REGISTRY_KEY;

/* get the object registry table */
static inline void luaC_object_registry_push(lua_State *L)
{
    lua_pushstring(L, LUAC_OBJECT_REGISTRY_KEY);
    lua_rawget(L, LUA_REGISTRYINDEX);
}

/* put a lua object at idx to the object registry with the pointer as key */
static inline int
luaC_object_register(lua_State *L, int idx, const void *pointer)
{
    lua_pushvalue(L, idx);
    luaC_object_registry_push(L);

    lua_pushlightuserdata(L, (void *)pointer);
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);

    lua_pop(L, 2);

    return 0;
}

/* remove a referenced object from the object registry */
static inline int luaC_object_unregister(lua_State *L, const void *pointer)
{
    luaC_object_registry_push(L);

    lua_pushlightuserdata(L, (void *)pointer);
    lua_pushnil(L);
    lua_rawset(L, -3);

    lua_pop(L, 1);

    return 0;
}

/** Push a referenced object onto the stack.
 * \param L The Lua VM state.
 * \param pointer The object to push.
 * \return The number of element pushed on stack.
 */
static inline int luaC_object_push(lua_State *L, const void *pointer)
{
    luaC_object_registry_push(L);
    lua_pushlightuserdata(L, (void *)pointer);
    lua_rawget(L, -2);
    lua_remove(L, -2);
    return 1;
}

/** Check if object exist in the registry table.
 * \param L The Lua VM state.
 * \param pointer The object to check.
 * \return Existence of the object.
 */
static inline bool luaC_object_valid(lua_State *L, const void *pointer)
{
    luaC_object_push(L, pointer);
    bool valid = !lua_isnil(L, -1);
    lua_pop(L, 1);
    return valid;
}

#endif // !_CWC_LUAOBJECT_H
