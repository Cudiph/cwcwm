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
 * @inputmodule cwc.kbd
 */

#include <lauxlib.h>
#include <linux/input-event-codes.h>
#include <lua.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/session.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

#include "cwc/config.h"
#include "cwc/desktop/output.h"
#include "cwc/desktop/session_lock.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/input/keyboard.h"
#include "cwc/input/seat.h"
#include "cwc/luaclass.h"
#include "cwc/luaobject.h"
#include "cwc/server.h"
#include "cwc/signal.h"
#include "cwc/util.h"

#define GENERATED_KEY_LENGTH 8
uint64_t keybind_generate_key(uint32_t modifiers, uint32_t keysym)
{
    uint64_t _key = 0;
    _key          = modifiers;
    _key          = (_key << 32) | keysym;
    return _key;
}

static bool _keybind_execute(struct cwc_keybind_map *kmap,
                             struct cwc_keybind_info *info,
                             bool press);

static int repeat_loop(void *data)
{
    struct cwc_keybind_map *kmap = data;

    _keybind_execute(kmap, kmap->repeated_bind, true);

    wl_event_source_timer_update(kmap->repeat_timer,
                                 2000 / g_config.repeat_rate);

    return 1;
}

static void _register_kmap_object(void *data)
{
    struct cwc_keybind_map *kmap = data;

    luaC_object_kbindmap_register(g_config_get_lua_State(), kmap);
}

struct cwc_keybind_map *cwc_keybind_map_create(struct wl_list *list)
{
    struct cwc_keybind_map *kmap = calloc(1, sizeof(*kmap));
    kmap->map                    = cwc_hhmap_create(0);
    kmap->active                 = true;
    kmap->repeat_timer =
        wl_event_loop_add_timer(server.wl_event_loop, repeat_loop, kmap);

    if (list)
        wl_list_insert(list->prev, &kmap->link);
    else
        wl_list_init(&kmap->link);

    if (g_config_get_lua_State()) {
        _register_kmap_object(kmap);
    } else {
        /* There's a circular dependency in server_init function so this need to
         * run after lua is initialized */
        wl_event_loop_add_idle(server.wl_event_loop, _register_kmap_object,
                               kmap);
    }

    return kmap;
}

void cwc_keybind_map_destroy(struct cwc_keybind_map *kmap)
{
    luaC_object_unregister(g_config_get_lua_State(), kmap);

    cwc_keybind_map_clear(kmap);
    cwc_hhmap_destroy(kmap->map);
    wl_event_source_remove(kmap->repeat_timer);

    wl_list_remove(&kmap->link);

    free(kmap);
}

/* Lifetime for group & description in C keybind must be static,
 * and for lua it need to be in the heap.
 */
static void cwc_keybind_info_destroy(struct cwc_keybind_info *info)
{
    lua_State *L = g_config_get_lua_State();
    luaC_object_unregister(L, info);
    switch (info->type) {
    case CWC_KEYBIND_TYPE_C:
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

    free(info);
}

static void _keybind_clear(struct cwc_hhmap *kmap)
{
    for (size_t i = 0; i < kmap->alloc; i++) {
        struct hhash_entry *elem = &kmap->table[i];
        if (!elem->hash)
            continue;
        cwc_keybind_info_destroy(elem->data);
    }
}

void cwc_keybind_map_clear(struct cwc_keybind_map *kmap)
{
    struct cwc_hhmap **map = &kmap->map;
    _keybind_clear(*map);
    cwc_hhmap_destroy(*map);
    *map = cwc_hhmap_create(8);
}

static void _keybind_remove_if_exist(struct cwc_keybind_map *kmap,
                                     uint64_t generated_key);

static void _keybind_register(struct cwc_keybind_map *kmap,
                              uint32_t modifiers,
                              uint32_t key,
                              struct cwc_keybind_info info)
{
    uint64_t generated_key = keybind_generate_key(modifiers, key);

    struct cwc_keybind_info *info_dup = malloc(sizeof(*info_dup));
    memcpy(info_dup, &info, sizeof(*info_dup));
    info_dup->key = generated_key;

    _keybind_remove_if_exist(kmap, generated_key);

    cwc_hhmap_ninsert(kmap->map, &generated_key, GENERATED_KEY_LENGTH,
                      info_dup);

    luaC_object_kbind_register(g_config_get_lua_State(), info_dup);
}

void keybind_kbd_register(struct cwc_keybind_map *kmap,
                          uint32_t modifiers,
                          xkb_keysym_t key,
                          struct cwc_keybind_info info)
{
    _keybind_register(kmap, modifiers, key, info);
}

void keybind_mouse_register(struct cwc_keybind_map *kmap,
                            uint32_t modifiers,
                            uint32_t button,
                            struct cwc_keybind_info info)
{
    _keybind_register(kmap, modifiers, button, info);
}

static void _keybind_remove_if_exist(struct cwc_keybind_map *kmap,
                                     uint64_t generated_key)
{
    struct cwc_keybind_info *existed =
        cwc_hhmap_nget(kmap->map, &generated_key, GENERATED_KEY_LENGTH);
    if (!existed)
        return;

    cwc_keybind_info_destroy(existed);
    cwc_hhmap_nremove(kmap->map, &generated_key, GENERATED_KEY_LENGTH);
}

void keybind_kbd_remove(struct cwc_keybind_map *kmap,
                        uint32_t modifiers,
                        xkb_keysym_t key)
{
    uint64_t generated_key = keybind_generate_key(modifiers, key);
    _keybind_remove_if_exist(server.main_kbd_kmap, generated_key);
}

void keybind_mouse_remove(struct cwc_keybind_map *kmap,
                          uint32_t modifiers,
                          uint32_t button)
{
    uint64_t generated_key = keybind_generate_key(modifiers, button);
    _keybind_remove_if_exist(server.main_mouse_kmap, generated_key);
}

static bool _keybind_execute(struct cwc_keybind_map *kmap,
                             struct cwc_keybind_info *info,
                             bool press)
{
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

    if (press && info->repeat && !kmap->repeated_bind) {
        kmap->repeated_bind = info;
        wl_event_source_timer_update(kmap->repeat_timer, g_config.repeat_delay);
    }

    return true;
}

bool keybind_kbd_execute(struct cwc_keybind_map *kmap,
                         struct cwc_seat *seat,
                         uint32_t modifiers,
                         xkb_keysym_t key,
                         bool press)
{
    uint64_t generated_key = keybind_generate_key(modifiers, key);
    struct cwc_keybind_info *info =
        cwc_hhmap_nget(kmap->map, &generated_key, GENERATED_KEY_LENGTH);

    if (info == NULL)
        return false;

    if (!info->exclusive
        && (server.session_lock->locked || seat->kbd_inhibitor))
        return false;

    return _keybind_execute(kmap, info, press);
}

bool keybind_mouse_execute(struct cwc_keybind_map *kmap,
                           uint32_t modifiers,
                           uint32_t button,
                           bool press)
{
    uint64_t generated_key = keybind_generate_key(modifiers, button);
    struct cwc_keybind_info *info =
        cwc_hhmap_nget(kmap->map, &generated_key, GENERATED_KEY_LENGTH);

    if (info == NULL)
        return false;

    return _keybind_execute(kmap, info, press);
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

void dump_keybinds_info(struct cwc_keybind_map *kmap)
{
    struct cwc_hhmap *map = kmap->map;
    for (size_t i = 0; i < map->alloc; i++) {
        struct hhash_entry *elem = &map->table[i];
        if (!elem->hash)
            continue;

        struct cwc_keybind_info *info = elem->data;

        if (!info->description)
            continue;

        char mods[101];
        mods[0]         = 0;
        char keysym[65] = {0};

        wlr_modifier_to_string(kbindinfo_key_get_modifier(info->key), mods,
                               100);
        xkb_keysym_get_name(kbindinfo_key_get_keysym(info->key), keysym, 64);
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
    // keybind_kbd_register(WLR_MODIFIER_NONE, XKB_KEY_F11,
    //                      (struct cwc_keybind_info){
    //                          .type     = CWC_KEYBIND_TYPE_C,
    //                          .on_press = _test,
    //                      });

    for (size_t i = 1; i <= 12; ++i) {
        char keyname[7];
        snprintf(keyname, 6, "F%ld", i);
        xkb_keysym_t key =
            xkb_keysym_from_name(keyname, XKB_KEYSYM_CASE_INSENSITIVE);
        keybind_kbd_register(server.main_kbd_kmap,
                             WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT, key,
                             (struct cwc_keybind_info){
                                 .type     = CWC_KEYBIND_TYPE_C,
                                 .on_press = _chvt,
                                 .args     = (void *)(i),
                             });
    }
}

//======================== LUA =============================

int cwc_keybind_map_register_bind_from_lua(lua_State *L,
                                           struct cwc_keybind_map *kmap)
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

        lua_getfield(L, data_index, "exclusive");
        info.exclusive = lua_toboolean(L, -1);

        lua_getfield(L, data_index, "repeated");
        info.repeat = lua_toboolean(L, -1);
    }

    // ready for register
    keybind_kbd_register(kmap, modifiers, keysym, info);

    return 0;
}

/** Register a keyboard binding to the default map.
 *
 * Keybinding registered in this map is always active and cannot be deactivated.
 *
 * @staticfct bind
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
 * @see cwc_kbindmap:bind
 */
static int luaC_kbd_bind(lua_State *L)
{
    return cwc_keybind_map_register_bind_from_lua(L, server.main_kbd_kmap);
}

/** Clear all keyboard binding in the default map.
 *
 * @staticfct clear
 * @tparam[opt=false] boolean common_key Also clear common key (chvt key)
 * @noreturn
 */
static int luaC_kbd_clear(lua_State *L)
{
    cwc_keybind_map_clear(server.main_kbd_kmap);
    return 0;
}

/** Create a new keybind map.
 *
 * @staticfct create_bindmap
 * @treturn cwc_kbindmap
 */
static int luaC_kbd_create_bindmap(lua_State *L)
{
    struct cwc_keybind_map *kmap = cwc_keybind_map_create(&server.kbd_kmaps);

    luaC_object_push(L, kmap);

    return 1;
}

/** Get all cwc_kbindmap object in the server.
 *
 * @tfield cwc_kbindmap[] bindmap
 * @readonly
 * @see default_member
 */
static int luaC_kbd_get_bindmap(lua_State *L)
{
    lua_newtable(L);

    int i = 1;
    struct cwc_keybind_map *input_dev;
    wl_list_for_each(input_dev, &server.kbd_kmaps, link)
    {
        luaC_object_push(L, input_dev);
        lua_rawseti(L, -2, i++);
    }

    return 1;
}

/** Get keybind object in the default map.
 *
 * @tfield cwc_kbind[] default_member
 * @readonly
 * @see cwc_kbindmap:member
 */
static int luaC_kbd_get_default_member(lua_State *L)
{
    struct cwc_keybind_map *kmap = server.main_kbd_kmap;

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

/** Keyboard repeat rate in hz.
 *
 * @tfield integer repeat_rate
 * @noreturn
 */
static int luaC_kbd_get_repeat_rate(lua_State *L)
{
    lua_pushnumber(L, g_config.repeat_rate);
    return 1;
}
static int luaC_kbd_set_repeat_rate(lua_State *L)
{
    int rate             = luaL_checkint(L, 1);
    g_config.repeat_rate = rate;
    return 0;
}

/** Keyboard repeat delay in miliseconds.
 *
 * @tfield integer repeat_delay
 * @noreturn
 */
static int luaC_kbd_get_repeat_delay(lua_State *L)
{
    lua_pushnumber(L, g_config.repeat_delay);
    return 1;
}
static int luaC_kbd_set_repeat_delay(lua_State *L)
{
    int delay             = luaL_checkint(L, 1);
    g_config.repeat_delay = delay;
    return 0;
}

#define FIELD_RO(name)     {"get_" #name, luaC_kbd_get_##name}
#define FIELD_SETTER(name) {"set_" #name, luaC_kbd_set_##name}
#define FIELD(name)        FIELD_RO(name), FIELD_SETTER(name)

void luaC_kbd_setup(lua_State *L)
{
    luaL_Reg keyboard_staticlibs[] = {
        {"bind",           luaC_kbd_bind          },
        {"clear",          luaC_kbd_clear         },
        {"create_bindmap", luaC_kbd_create_bindmap},

        FIELD_RO(bindmap),
        FIELD_RO(default_member),

        FIELD(repeat_rate),
        FIELD(repeat_delay),

        {NULL,             NULL                   },
    };

    luaC_register_table(L, "cwc.kbd", keyboard_staticlibs, NULL);
    lua_setfield(L, -2, "kbd");
}
