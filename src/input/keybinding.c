/* keybinding.c - keybinding module
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

/** Low-level API to manage keyboard behavior
 *
 * @author Dwi Asmoro Bangun
 * @copyright 2024
 * @license GPLv3
 * @coreclassmod cwc.kbd
 */

#include <lauxlib.h>
#include <linux/input-event-codes.h>
#include <lua.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/session.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

#include "cwc/config.h"
#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/input/keyboard.h"
#include "cwc/server.h"
#include "cwc/util.h"

#define GENERATED_KEY_LENGTH 8
uint64_t keybind_generate_key(uint32_t modifiers, uint32_t keysym)
{
    uint64_t _key = 0;
    _key          = modifiers;
    _key          = (_key << 32) | keysym;
    return _key;
}

static inline uint32_t generated_key_get_modifier(uint64_t genkey)
{
    return genkey >> 32;
}

static inline uint32_t generated_key_get_key(uint64_t genkey)
{
    return genkey & 0xffffffff;
}

void __keybind_register(struct cwc_hhmap *map,
                        uint32_t modifiers,
                        uint32_t key,
                        struct cwc_keybind_info info)
{
    uint64_t generated_key = keybind_generate_key(modifiers, key);

    struct cwc_keybind_info *info_dup = malloc(sizeof(*info_dup));
    memcpy(info_dup, &info, sizeof(*info_dup));
    info_dup->key = generated_key;

    __keybind_remove_if_exist(map, generated_key);

    cwc_hhmap_ninsert(map, &generated_key, GENERATED_KEY_LENGTH, info_dup);
}

void keybind_kbd_register(uint32_t modifiers,
                          xkb_keysym_t key,
                          struct cwc_keybind_info info)
{
    __keybind_register(server.keybind_kbd_map, modifiers, key, info);
}

void keybind_mouse_register(uint32_t modifiers,
                            uint32_t button,
                            struct cwc_keybind_info info)
{
    __keybind_register(server.keybind_mouse_map, modifiers, button, info);
}

/* Lifetime for group & description in C keybind must be static,
 * and for lua it need to be in the heap.
 */
static void clean_info(struct cwc_keybind_info *info)
{
    lua_State *L = g_config_get_lua_State();
    switch (info->type) {
    case CWC_KEYBIND_TYPE_C:
        free(info);
        break;
    case CWC_KEYBIND_TYPE_LUA:
        if (info->luaref_press)
            luaL_unref(L, LUA_REGISTRYINDEX, info->luaref_press);
        if (info->luaref_release)
            luaL_unref(L, LUA_REGISTRYINDEX, info->luaref_release);

        free(info->group);
        free(info->description);
        break;
    }
}

void __keybind_remove_if_exist(struct cwc_hhmap *map, uint64_t generated_key)
{
    struct cwc_keybind_info *existed =
        cwc_hhmap_nget(map, &generated_key, GENERATED_KEY_LENGTH);
    if (!existed)
        return;

    clean_info(existed);
    cwc_hhmap_nremove(map, &generated_key, GENERATED_KEY_LENGTH);
}

void keybind_kbd_remove(uint32_t modifiers, xkb_keysym_t key)
{
    uint64_t generated_key = keybind_generate_key(modifiers, key);
    __keybind_remove_if_exist(server.keybind_kbd_map, generated_key);
}

void keybind_mouse_remove(uint32_t modifiers, uint32_t button)
{
    uint64_t generated_key = keybind_generate_key(modifiers, button);
    __keybind_remove_if_exist(server.keybind_mouse_map, generated_key);
}

bool __keybind_execute(struct cwc_hhmap *map,
                       uint32_t modifiers,
                       xkb_keysym_t key,
                       bool press)
{
    uint64_t generated_key = keybind_generate_key(modifiers, key);

    struct cwc_keybind_info *info =
        cwc_hhmap_nget(map, &generated_key, GENERATED_KEY_LENGTH);

    if (info == NULL)
        return false;

    lua_State *L = g_config_get_lua_State();
    int idx;
    switch (info->type) {
    case CWC_KEYBIND_TYPE_LUA:
        if (press)
            idx = info->luaref_press;
        else
            idx = info->luaref_release;

        if (!idx)
            break;

        lua_rawgeti(L, LUA_REGISTRYINDEX, idx);
        if (lua_pcall(L, 0, 0, 0))
            cwc_log(CWC_ERROR, "error when executing keybind: %s",
                    lua_tostring(L, -1));
        break;
    case CWC_KEYBIND_TYPE_C:
        if (press && info->on_press) {
            info->on_press(info->args);
        } else if (info->on_release) {
            info->on_release(info->args);
        }

        break;
    }

    return true;
}

bool keybind_kbd_execute(uint32_t modifiers, xkb_keysym_t key, bool press)
{
    return __keybind_execute(server.keybind_kbd_map, modifiers, key, press);
}

bool keybind_mouse_execute(uint32_t modifiers, uint32_t button, bool press)
{
    return __keybind_execute(server.keybind_mouse_map, modifiers, button,
                             press);
}

static void __keybind_clear(struct cwc_hhmap *map)
{
    for (size_t i = 0; i < map->alloc; i++) {
        struct hhash_entry *elem = &map->table[i];
        if (!elem->hash)
            continue;
        clean_info(elem->data);
    }
}

void keybind_kbd_clear(bool clear_common_key)
{
    struct cwc_hhmap **map = &server.keybind_kbd_map;
    __keybind_clear(*map);
    cwc_hhmap_destroy(*map);
    *map = cwc_hhmap_create(8);
    if (!clear_common_key)
        keybind_register_common_key();
}

void keybind_mouse_clear()
{
    struct cwc_hhmap **map = &server.keybind_mouse_map;
    __keybind_clear(*map);
    cwc_hhmap_destroy(*map);
    *map = cwc_hhmap_create(8);
}

static void wlr_modifier_to_string(uint32_t mod, char *str, int len)
{
    if (mod & WLR_MODIFIER_LOGO)
        strncat(str, "Super + ", len - 1);

    if (mod & WLR_MODIFIER_CTRL)
        strncat(str, "Control + ", len - 1);

    if (mod & WLR_MODIFIER_ALT)
        strncat(str, "Alt + ", len - 1);

    if (mod & WLR_MODIFIER_SHIFT)
        strncat(str, "Shift + ", len - 1);

    if (mod & WLR_MODIFIER_CAPS)
        strncat(str, "Caps + ", len - 1);

    if (mod & WLR_MODIFIER_MOD2)
        strncat(str, "Mod2 + ", len - 1);

    if (mod & WLR_MODIFIER_MOD3)
        strncat(str, "Mod3 + ", len - 1);

    if (mod & WLR_MODIFIER_MOD5)
        strncat(str, "Mod5 + ", len - 1);
}

void dump_keybinds_info()
{
    struct cwc_hhmap *map = server.keybind_kbd_map;
    for (size_t i = 0; i < map->alloc; i++) {
        struct hhash_entry *elem = &map->table[i];
        if (!elem->hash)
            continue;

        struct cwc_keybind_info *info = elem->data;

        if (!info->description)
            continue;

        char mods[100];
        mods[0]         = 0;
        char keysym[64] = {0};

        wlr_modifier_to_string(generated_key_get_modifier(info->key), mods,
                               100);
        xkb_keysym_get_name(generated_key_get_key(info->key), keysym, 64);
        printf("%s\t%s%s\t\t%s\n", info->group, mods, keysym,
               info->description);
    }
}

static void _chvt(void *args)
{
    wlr_session_change_vt(server.session, (uint64_t)args);
}

static void _test(void *args)
{
    ;
}

#define WLR_MODIFIER_NONE 0
void keybind_register_common_key()
{
    keybind_kbd_register(WLR_MODIFIER_NONE, XKB_KEY_F11,
                         (struct cwc_keybind_info){
                             .type     = CWC_KEYBIND_TYPE_C,
                             .on_press = _test,
                         });

    for (size_t i = 1; i <= 12; ++i) {
        char keyname[7];
        snprintf(keyname, 6, "F%ld", i);
        xkb_keysym_t key =
            xkb_keysym_from_name(keyname, XKB_KEYSYM_CASE_INSENSITIVE);
        keybind_kbd_register(WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT, key,
                             (struct cwc_keybind_info){
                                 .type     = CWC_KEYBIND_TYPE_C,
                                 .on_press = _chvt,
                                 .args     = (void *)(i),
                             });
    }
}

//======================== LUA =============================

/** Register a keyboard binding.
 *
 * @staticfct bind
 * @tparam table|number modifier Table of modifier or modifier bitfield
 * @tparam string keyname Keyname from `xkbcommon-keysyms.h`
 * @tparam func on_press Function to execute when pressed
 * @tparam[opt] func on_release Function to execute when released
 * @tparam[opt] table data Additional data
 * @tparam[opt] string data.group Keybinding group
 * @tparam[opt] string data.description Keybinding description
 * @noreturn
 * @see cuteful.enum.modifier
 * @see cwc.pointer.bind
 */
static int luaC_kbd_bind(lua_State *L)
{
    if (!lua_isnumber(L, 2) && !lua_isstring(L, 2))
        luaL_error(L, "Key can only be a string or number");

    luaL_checktype(L, 3, LUA_TFUNCTION);

    // process the modifier table
    uint32_t modifiers = 0;
    if (lua_istable(L, 1)) {
        int len = lua_objlen(L, 1);

        for (int i = 0; i < len; ++i) {
            lua_rawgeti(L, 1, i + 1);
            modifiers |= luaL_checkint(L, -1);
        }

    } else if (lua_isnumber(L, 1)) {
        modifiers = lua_tonumber(L, 1);
    } else {
        luaL_error(L,
                   "modifiers only accept array of number or modifier bitmask");
    }

    // process key
    xkb_keysym_t keysym;
    if (lua_type(L, 2) == LUA_TNUMBER) {
        keysym = lua_tointeger(L, 2);
    } else {
        const char *keyname = luaL_checkstring(L, 2);
        keysym = xkb_keysym_from_name(keyname, XKB_KEYSYM_CASE_INSENSITIVE);

        if (keysym == XKB_KEY_NoSymbol) {
            luaL_error(L, "no such key \"%s\"", keyname);
            return 0;
        }
    }

    // process press/release callback
    bool on_press_is_function   = lua_isfunction(L, 3);
    bool on_release_is_function = lua_isfunction(L, 4);

    if (!on_press_is_function && !on_release_is_function) {
        luaL_error(L, "callback function is not provided");
        return 0;
    }

    struct cwc_keybind_info info = {0};
    info.type                    = CWC_KEYBIND_TYPE_LUA;

    if (on_press_is_function) {
        lua_pushvalue(L, 3);
        info.luaref_press = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    int data_index;
    if (on_release_is_function) {
        lua_pushvalue(L, 4);
        info.luaref_release = luaL_ref(L, LUA_REGISTRYINDEX);
        data_index          = 5;
    } else {
        data_index = 4;
    }

    // save the keybind data
    if (lua_istable(L, data_index)) {
        lua_getfield(L, data_index, "description");
        if (lua_isstring(L, -1))
            info.description = strdup(lua_tostring(L, -1));

        lua_getfield(L, data_index, "group");
        if (lua_isstring(L, -1))
            info.group = strdup(lua_tostring(L, -1));
    }

    // ready for register
    keybind_kbd_register(modifiers, keysym, info);

    return 0;
}

/** Clear all keyboard binding
 *
 * @staticfct clear
 * @tparam[opt=false] boolean common_key Also clear common key (chvt key)
 * @noreturn
 */
static int luaC_kbd_clear(lua_State *L)
{
    if (lua_isboolean(L, 1)) {
        bool clear_common_key = lua_toboolean(L, 1);
        if (clear_common_key) {
            keybind_kbd_clear(true);
            return 0;
        }
    }

    keybind_kbd_clear(false);
    return 0;
}

/** Set keyboard repeat rate
 *
 * @configfct set_repeat_rate
 * @tparam number rate Rate in hertz
 * @noreturn
 */
static int luaC_kbd_set_repeat_rate(lua_State *L)
{
    int rate             = luaL_checkint(L, 1);
    g_config.repeat_rate = rate;
    return 0;
}

/** Set keyboard repeat delay
 *
 * @configfct set_repeat_delay
 * @tparam number delay Delay in miliseconds
 * @noreturn
 */
static int luaC_kbd_set_repeat_delay(lua_State *L)
{
    int delay             = luaL_checkint(L, 1);
    g_config.repeat_delay = delay;
    return 0;
}

void luaC_kbd_setup(lua_State *L)
{
    luaL_Reg keyboard_staticlibs[] = {
        {"bind",             luaC_kbd_bind            },
        {"clear",            luaC_kbd_clear           },

        {"set_repeat_rate",  luaC_kbd_set_repeat_rate },
        {"set_repeat_delay", luaC_kbd_set_repeat_delay},
        {NULL,               NULL                     },
    };

    lua_newtable(L);
    luaL_register(L, NULL, keyboard_staticlibs);
    lua_setfield(L, -2, "kbd");
}
