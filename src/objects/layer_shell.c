/* layer_shell.c - lua cwc_layer_shell object
 *
 * Copyright (C) 2025 Dwi Asmoro Bangun <dwiaceromo@gmail.com>
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

/** Layer shell object info and events.
 *
 * @author Dwi Asmoro Bangun
 * @copyright 2025
 * @license GPLv3
 * @coreclassmod cwc.layer_shell
 */

#include <lauxlib.h>
#include <libinput.h>
#include <lua.h>

#include "cwc/desktop/layer_shell.h"
#include "cwc/desktop/output.h"
#include "cwc/luaclass.h"
#include "cwc/luaobject.h"
#include "cwc/server.h"

/** Emitted when a layer shell surface is created.
 *
 * @signal layer_shell::new
 * @tparam cwc_layer_shell layer_shell The layer shell object.
 */

/** Emitted when layer shell surface is about to be destroyed.
 *
 * @signal layer_shell::destroy
 * @tparam cwc_layer_shell layer_shell The layer shell object.
 */

//================== CODE ====================

/** Get all input device in the server.
 *
 * @staticfct get
 * @treturn cwc_layer_shell[]
 */
static int luaC_layer_shell_get(lua_State *L)
{
    lua_newtable(L);

    int i = 1;
    struct cwc_layer_surface *lshell;
    wl_list_for_each(lshell, &server.layer_shells, link)
    {
        luaC_object_push(L, lshell);
        lua_rawseti(L, -2, i++);
    }

    return 1;
}

/** Kill a layer shell client without any question asked.
 *
 * @method kill
 * @noreturn
 */
static int luaC_layer_shell_kill(lua_State *L)
{
    struct cwc_layer_surface *layer_shell = luaC_layer_shell_checkudata(L, 1);
    wl_client_destroy(layer_shell->wlr_layer_surface->resource->client);
    return 0;
}

/** The screen where the layer shell visible.
 *
 * @property screen
 * @tparam cwc_screen screen
 * @readonly
 * @propertydefault Up to the client.
 */
static int luaC_layer_shell_get_screen(lua_State *L)
{
    struct cwc_layer_surface *layer_shell = luaC_layer_shell_checkudata(L, 1);
    luaC_object_push(L, layer_shell->output);
    return 1;
}

/** The namespace of the layer shell.
 *
 * @property namespace
 * @tparam string namespace
 * @readonly
 * @propertydefault Extracted from wlr_layer_surface.
 */
static int luaC_layer_shell_get_namespace(lua_State *L)
{
    struct cwc_layer_surface *layer_shell = luaC_layer_shell_checkudata(L, 1);
    lua_pushstring(L, layer_shell->wlr_layer_surface->namespace);
    return 1;
}

/** The pid of the layer shell.
 *
 * @property pid
 * @tparam number pid
 * @readonly
 * @negativeallowed false
 * @propertydefault Provided by the OS.
 */
static int luaC_layer_shell_get_pid(lua_State *L)
{
    struct cwc_layer_surface *layer_shell = luaC_layer_shell_checkudata(L, 1);
    pid_t pid                             = 0;

    wl_client_get_credentials(layer_shell->wlr_layer_surface->resource->client,
                              &pid, NULL, NULL);

    lua_pushnumber(L, pid);
    return 1;
}

#define REG_METHOD(name)    {#name, luaC_layer_shell_##name}
#define REG_READ_ONLY(name) {"get_" #name, luaC_layer_shell_get_##name}

void luaC_layer_shell_setup(lua_State *L)
{
    luaL_Reg layer_shell_metamethods[] = {
        {"__eq",       luaC_layer_shell_eq      },
        {"__tostring", luaC_layer_shell_tostring},
        {NULL,         NULL                     },
    };

    luaL_Reg layer_shell_methods[] = {
        REG_METHOD(kill),

        REG_READ_ONLY(screen), REG_READ_ONLY(namespace), REG_READ_ONLY(pid),

        {NULL, NULL},
    };

    luaC_register_class(L, layer_shell_classname, layer_shell_methods,
                        layer_shell_metamethods);
    luaL_Reg layer_shell_staticlibs[] = {
        {"get", luaC_layer_shell_get},
        {NULL,  NULL                },
    };

    luaC_register_table(L, "cwc.layer_shell", layer_shell_staticlibs, NULL);
    lua_setfield(L, -2, "layer_shell");
}
