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
#include <wlr/types/wlr_keyboard_group.h>
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
#include "cwc/process.h"
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

void keybind_register(struct cwc_keybind_map *kmap,
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

void keybind_remove(struct cwc_keybind_map *kmap,
                    uint32_t modifiers,
                    uint32_t key)
{
    uint64_t generated_key = keybind_generate_key(modifiers, key);
    _keybind_remove_if_exist(kmap, generated_key);
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

    if (info->pass)
        return false;

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

static void on_ioready(struct spawn_obj *spawn_obj,
                       const char *out,
                       const char *err,
                       void *data)
{
    if (out) {
        printf("out: %s", out);
    } else {
        printf("err: %s", err);
    }
    if (data == &_chvt)
        puts("benarrr");
}

static void on_exited(struct spawn_obj *spawn_obj, int exit_code, void *data)
{
    printf("eee %d\n", exit_code);
}

static void _test(void *args)
{
    // struct cwc_process_callback_info info = {
    //     .type       = CWC_PROCESS_TYPE_C,
    //     .on_ioready = on_ioready,
    //     .on_exited  = on_exited,
    //     .data       = &_chvt,
    // };
    //
    // spawn_with_shell_easy_async(
    //     "echo bbbbb && sleep 1 && echo aaaaaa 1>&2 && exit 3", info);
}

#define WLR_MODIFIER_NONE 0
void keybind_register_common_key()
{
    // keybind_register(server.main_kbd_kmap, WLR_MODIFIER_NONE, XKB_KEY_F11,
    //                  (struct cwc_keybind_info){
    //                      .type     = CWC_KEYBIND_TYPE_C,
    //                      .on_press = _test,
    //                  });

    for (size_t i = 1; i <= 12; ++i) {
        char keyname[7];
        snprintf(keyname, 6, "F%ld", i);
        xkb_keysym_t key =
            xkb_keysym_from_name(keyname, XKB_KEYSYM_CASE_INSENSITIVE);
        keybind_register(server.main_kbd_kmap,
                         WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT, key,
                         (struct cwc_keybind_info){
                             .type     = CWC_KEYBIND_TYPE_C,
                             .on_press = _chvt,
                             .args     = (void *)(i),
                         });
    }
}
