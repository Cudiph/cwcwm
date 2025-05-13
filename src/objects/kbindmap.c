/* kbindmap.c - keybinding map object
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

/** Lua object for keybindings map.
 *
 * @author Dwi Asmoro Bangun
 * @copyright 2025
 * @license GPLv3
 * @coreclassmod cwc_kbindmap
 */

#include <lauxlib.h>
#include <libinput.h>
#include <lua.h>
#include <wayland-util.h>

#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/input/keyboard.h"
#include "cwc/luaclass.h"
#include "cwc/server.h"

/** Active state of the keybind map.
 *
 * @property active
 * @tparam[opt=true] boolean active
 */
static int luaC_kbindmap_get_active(lua_State *L)
{
    struct cwc_keybind_map *kmap = luaC_kbindmap_checkudata(L, 1);

    lua_pushboolean(L, kmap->active);

    return 1;
}

static int luaC_kbindmap_set_active(lua_State *L)
{
    struct cwc_keybind_map *kmap = luaC_kbindmap_checkudata(L, 1);

    kmap->active = lua_toboolean(L, 2);

    return 0;
}

/** Get list of all binding in the map.
 *
 * @property member
 * @readonly
 * @tparam[opt={}] cwc_kbind[] member List of the bindings.
 * @see cwc_kbind
 * @see cwc.kbd.get_default_member
 */
static int luaC_kbindmap_get_member(lua_State *L)
{
    struct cwc_keybind_map *kmap = luaC_kbindmap_checkudata(L, 1);

    lua_newtable(L);
    int j = 1;
    for (size_t i = 0; i < kmap->map->alloc; i++) {
        struct hhash_entry *elem = &kmap->map->table[i];
        if (!elem->hash)
            continue;
        struct cwc_keybind_info *kbind = elem->data;

        luaC_object_push(L, kbind);
        lua_rawseti(L, -2, j++);
    }

    return 1;
}

/** Register a keyboard binding.
 *
 * @method bind
 * @tparam table|number modifier Table of modifier or modifier bitfield
 * @tparam string keyname Keyname from `xkbcommon-keysyms.h`
 * @tparam func on_press Function to execute when pressed
 * @tparam[opt] func on_release Function to execute when released
 * @tparam[opt] table data Additional data
 * @tparam[opt] string data.group Keybinding group
 * @tparam[opt] string data.description Keybinding description
 * @tparam[opt] string data.exclusive Allow keybind to be executed even in
 * lockscreen and shortcut inhibit
 * @tparam[opt] string data.repeated Repeat keybind when hold (only on_press
 * will be executed)
 * @noreturn
 * @see cuteful.enum.modifier
 * @see cwc.pointer.bind
 * @see cwc.kbd.bind
 */
static int luaC_kbindmap_bind(lua_State *L)
{
    struct cwc_keybind_map *kmap = luaC_kbindmap_checkudata(L, 1);

    /* remove the kmap object so that the argument is equal to in cwc.kbd.bind
     */
    lua_remove(L, 1);

    cwc_keybind_map_register_bind_from_lua(L, kmap);

    return 0;
}

/** Set this map as the only active one disabling all others map.
 *
 * @method active_only
 * @noreturn
 */
static int luaC_kbindmap_active_only(lua_State *L)
{
    struct cwc_keybind_map *kmap = luaC_kbindmap_checkudata(L, 1);

    struct cwc_keybind_map *input_dev;
    wl_list_for_each(input_dev, &server.kbd_kmaps, link)
    {
        input_dev->active = false;
    }

    kmap->active = true;

    return 0;
}

/** Clear all keybinding (emptying the map).
 *
 * @method clear
 * @noreturn
 */
static int luaC_kbindmap_clear(lua_State *L)
{
    struct cwc_keybind_map *kmap = luaC_kbindmap_checkudata(L, 1);

    cwc_keybind_map_clear(kmap);

    return 0;
}

/** Destroy this map freeing it from memory.
 *
 * @method destroy
 * @noreturn
 */
static int luaC_kbindmap_destroy(lua_State *L)
{
    struct cwc_keybind_map *kmap = luaC_kbindmap_checkudata(L, 1);

    cwc_keybind_map_destroy(kmap);

    return 0;
}

#define REG_METHOD(name)    {#name, luaC_kbindmap_##name}
#define REG_READ_ONLY(name) {"get_" #name, luaC_kbindmap_get_##name}
#define REG_SETTER(name)    {"set_" #name, luaC_kbindmap_set_##name}
#define REG_PROPERTY(name)  REG_READ_ONLY(name), REG_SETTER(name)

void luaC_kbindmap_setup(lua_State *L)
{
    luaL_Reg kbindmap_metamethods[] = {
        {"__eq",       luaC_kbindmap_eq      },
        {"__tostring", luaC_kbindmap_tostring},
        {NULL,         NULL                  },
    };

    luaL_Reg kbindmap_methods[] = {
        REG_METHOD(bind),      REG_METHOD(active_only),
        REG_METHOD(clear),     REG_METHOD(destroy),

        REG_READ_ONLY(member),

        REG_PROPERTY(active),

        {NULL, NULL},
    };

    luaC_register_class(L, kbindmap_classname, kbindmap_methods,
                        kbindmap_metamethods);
}
