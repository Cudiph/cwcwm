/* keybinds.c - keybind information object
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

/** Lua object for registered keybind.
 *
 * @author Dwi Asmoro Bangun
 * @copyright 2025
 * @license GPLv3
 * @inputmodule cwc_kbind
 */

#include <lauxlib.h>
#include <libinput.h>
#include <lua.h>
#include <wayland-util.h>
#include <wlr/types/wlr_keyboard.h>
#include <xkbcommon/xkbcommon.h>

#include "cwc/input/keyboard.h"
#include "cwc/luaclass.h"

/** User defined description of the keybind.
 *
 * @property description
 * @tparam[opt=''] string description
 */
static int luaC_kbind_get_description(lua_State *L)
{
    struct cwc_keybind_info *kbind = luaC_kbind_checkudata(L, 1);

    lua_pushstring(L, kbind->description);

    return 1;
}
static int luaC_kbind_set_description(lua_State *L)
{
    struct cwc_keybind_info *kbind = luaC_kbind_checkudata(L, 1);

    free(kbind->description);

    if (lua_isstring(L, 2))
        kbind->description = strdup(lua_tostring(L, 2));
    else
        kbind->description = NULL;

    return 0;
}

/** User defined group name of the keybind.
 *
 * @property group
 * @tparam[opt=''] string group
 */
static int luaC_kbind_get_group(lua_State *L)
{
    struct cwc_keybind_info *kbind = luaC_kbind_checkudata(L, 1);

    lua_pushstring(L, kbind->group);

    return 1;
}
static int luaC_kbind_set_group(lua_State *L)
{
    struct cwc_keybind_info *kbind = luaC_kbind_checkudata(L, 1);

    free(kbind->group);

    if (lua_isstring(L, 2))
        kbind->group = strdup(lua_tostring(L, 2));
    else
        kbind->group = NULL;

    return 0;
}

/** Exclusivity option of the keybind.
 *
 * @property exclusive
 * @tparam[opt=false] boolean exclusive
 */
static int luaC_kbind_get_exclusive(lua_State *L)
{
    struct cwc_keybind_info *kbind = luaC_kbind_checkudata(L, 1);

    lua_pushboolean(L, kbind->exclusive);

    return 1;
}
static int luaC_kbind_set_exclusive(lua_State *L)
{
    luaL_checktype(L, 2, LUA_TBOOLEAN);
    struct cwc_keybind_info *kbind = luaC_kbind_checkudata(L, 1);

    kbind->exclusive = lua_toboolean(L, 2);

    return 0;
}

/** Repeat option of the keybind.
 *
 * @property repeated
 * @tparam[opt=false] boolean repeated
 */
static int luaC_kbind_get_repeated(lua_State *L)
{
    struct cwc_keybind_info *kbind = luaC_kbind_checkudata(L, 1);

    lua_pushboolean(L, kbind->repeat);

    return 1;
}
static int luaC_kbind_set_repeated(lua_State *L)
{
    luaL_checktype(L, 2, LUA_TBOOLEAN);
    struct cwc_keybind_info *kbind = luaC_kbind_checkudata(L, 1);

    kbind->repeat = lua_toboolean(L, 2);

    return 0;
}

/** Pass option of the keybind.
 *
 * @property pass
 * @tparam[opt=false] boolean pass
 */
static int luaC_kbind_get_pass(lua_State *L)
{
    struct cwc_keybind_info *kbind = luaC_kbind_checkudata(L, 1);

    lua_pushboolean(L, kbind->pass);

    return 1;
}
static int luaC_kbind_set_pass(lua_State *L)
{
    luaL_checktype(L, 2, LUA_TBOOLEAN);
    struct cwc_keybind_info *kbind = luaC_kbind_checkudata(L, 1);

    kbind->pass = lua_toboolean(L, 2);

    return 0;
}

/** Registered modifiers of this keybind.
 *
 * @property modifier
 * @readonly
 * @tparam[opt={}] number[] modifier List of modifiers
 * @see cuteful.enum.modifier
 */
static int luaC_kbind_get_modifier(lua_State *L)
{
    struct cwc_keybind_info *kbind = luaC_kbind_checkudata(L, 1);
    uint32_t mod                   = kbindinfo_key_get_modifier(kbind->key);

    lua_newtable(L);

    int i = 1;
    for (int j = 0; j < WLR_MODIFIER_COUNT; j++) {
        if (~mod & (1 << j))
            continue;

        lua_pushnumber(L, 1 << j);
        lua_rawseti(L, -2, i++);
    }

    return 1;
}

static const char *modname_table[] = {
    [0] = "SHIFT", [1] = "CAPS", [2] = "CTRL", [3] = "ALT",
    [4] = "MOD2",  [5] = "MOD3", [6] = "LOGO", [7] = "MOD5",
};

/** Like modifier but return the name.
 *
 * @property modifier_name
 * @readonly
 * @tparam[opt={}] string[] modifier_name List of modifier in human readable
 * @see cuteful.enum.modifier
 */
static int luaC_kbind_get_modifier_name(lua_State *L)
{
    struct cwc_keybind_info *kbind = luaC_kbind_checkudata(L, 1);
    uint32_t mod                   = kbindinfo_key_get_modifier(kbind->key);

    lua_newtable(L);

    int i = 1;
    for (int j = 0; j < WLR_MODIFIER_COUNT; j++) {
        if (~mod & (1 << j))
            continue;

        lua_pushstring(L, modname_table[j]);
        lua_rawseti(L, -2, i++);
    }

    return 1;
}

/** The name of the registered keysym.
 *
 * @property keyname
 * @readonly
 * @tparam[opt=''] string keyname The name of the key
 * @see cuteful.enum.modifier
 */
static int luaC_kbind_get_keyname(lua_State *L)
{
    struct cwc_keybind_info *kbind = luaC_kbind_checkudata(L, 1);
    uint32_t keysym                = kbindinfo_key_get_keysym(kbind->key);

    char keyname[65] = {0};
    xkb_keysym_get_name(keysym, keyname, 64);

    lua_pushstring(L, keyname);

    return 1;
}

/** Registered xkb keysym.
 *
 * @property keysym
 * @readonly
 * @tparam[opt=0] number keysym The keysym number
 * @negativeallowed false
 * @see cuteful.enum.modifier
 */
static int luaC_kbind_get_keysym(lua_State *L)
{
    struct cwc_keybind_info *kbind = luaC_kbind_checkudata(L, 1);
    uint32_t keysym                = kbindinfo_key_get_keysym(kbind->key);

    lua_pushnumber(L, keysym);

    return 1;
}

#define REG_METHOD(name)    {#name, luaC_kbind_##name}
#define REG_READ_ONLY(name) {"get_" #name, luaC_kbind_get_##name}
#define REG_SETTER(name)    {"set_" #name, luaC_kbind_set_##name}
#define REG_PROPERTY(name)  REG_READ_ONLY(name), REG_SETTER(name)

void luaC_kbind_setup(lua_State *L)
{
    luaL_Reg kbind_metamethods[] = {
        {"__eq",       luaC_kbind_eq      },
        {"__tostring", luaC_kbind_tostring},
        {NULL,         NULL               },
    };

    luaL_Reg kbind_methods[] = {
        REG_READ_ONLY(data),
        REG_READ_ONLY(modifier),
        REG_READ_ONLY(modifier_name),
        REG_READ_ONLY(keyname),
        REG_READ_ONLY(keysym),

        REG_PROPERTY(description),
        REG_PROPERTY(group),
        REG_PROPERTY(exclusive),
        REG_PROPERTY(repeated),
        REG_PROPERTY(pass),

        {NULL, NULL},
    };

    luaC_register_class(L, kbind_classname, kbind_methods, kbind_metamethods);
}
