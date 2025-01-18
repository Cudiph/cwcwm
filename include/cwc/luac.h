/*
 * luac.h - Lua configuration management header
 *
 * Copyright © 2008-2009 Julien Danjou <julien@danjou.info>
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

#ifndef _CWC_LUAC_H
#define _CWC_LUAC_H

#include <cairo.h>
#include <lauxlib.h>
#include <lua.h>
#include <stdbool.h>
#include <string.h>
#include <wlr/util/box.h>

extern bool lua_initial_load;
extern char *config_path;
extern char *library_path;

int luaC_init();
void luaC_fini();

//========== MACRO =============

static inline void luaC_dumpstack(lua_State *L)
{
    fprintf(stderr, "-------- Lua stack dump ---------\n");
    for (int i = lua_gettop(L); i; i--) {
        int t = lua_type(L, i);
        switch (t) {
        case LUA_TSTRING:
            fprintf(stderr, "%d: string\t\t`%s'\n", i, lua_tostring(L, i));
            break;
        case LUA_TBOOLEAN:
            fprintf(stderr, "%d: bool\t\t%s\n", i,
                    lua_toboolean(L, i) ? "true" : "false");
            break;
        case LUA_TNUMBER:
            fprintf(stderr, "%d: number\t\t%g\n", i, lua_tonumber(L, i));
            break;
        case LUA_TNIL:
            fprintf(stderr, "%d:\t\t nil\n", i);
            break;
        default:
            fprintf(stderr, "%d: %s\t#%d\t%p\n", i, lua_typename(L, t),
                    (int)lua_objlen(L, i), lua_topointer(L, i));
            break;
        }
    }
    fprintf(stderr, "------- Lua stack dump end ------\n");
}

/* push a table with the structure of wlr_box to the stack */
static inline int luaC_pushbox(lua_State *L, struct wlr_box box)
{
    lua_createtable(L, 0, 4);
    lua_pushnumber(L, box.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, box.y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, box.width);
    lua_setfield(L, -2, "width");
    lua_pushnumber(L, box.height);
    lua_setfield(L, -2, "height");

    return 1;
}

/* check if the color is a cairo pattern */
static inline cairo_pattern_t *luaC_checkcolor(lua_State *L, int idx)
{
    if (idx < 0)
        idx = lua_gettop(L) + idx + 1;

    luaL_checktype(L, idx, LUA_TUSERDATA);
    int last_stack_size = lua_gettop(L);

    // tostring(arg1)
    lua_getglobal(L, "tostring");
    lua_pushvalue(L, idx);
    lua_pcall(L, 1, 1, 0);

    // check if returned string has word cairo
    if (!strstr(lua_tostring(L, -1), "cairo")) {
        luaL_error(L, "color need to be created from gears.color");
        return NULL;
    }

    cairo_pattern_t **pattern = lua_touserdata(L, idx);

    lua_settop(L, last_stack_size);
    return *pattern;
}

#endif // !_CWC_LUAC_H
