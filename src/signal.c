/* signal.c - cwc's signal management
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

/* List C signal that only emitted to the C listener:
 *
 * `lua::reload` (NULL) - the lua_State is reinitialize, if you save object in
 * the lua state you need to register it back.
 *
 * `cwc::shutdown` (NULL) - the event loop has exited and you may need to do
 * some cleaning up
 */

#include <stdarg.h>
#include <stdlib.h>
#include <wayland-util.h>

#include "cwc/config.h"
#include "cwc/luaobject.h"
#include "cwc/server.h"
#include "cwc/signal.h"
#include "cwc/util.h"
#include "lauxlib.h"
#include "lua.h"

/* the entry in won't be deleted once create */
static struct cwc_signal_entry *
get_signal_entry_or_create_if_not_exist(const char *name)
{
    struct cwc_signal_entry *sig_entry = cwc_hhmap_get(server.signal_map, name);
    if (sig_entry)
        return sig_entry;

    sig_entry = malloc(sizeof(*sig_entry));
    wl_list_init(&sig_entry->c_callbacks);
    wl_list_init(&sig_entry->lua_callbacks);
    cwc_hhmap_insert(server.signal_map, name, sig_entry);

    return sig_entry;
}

void cwc_signal_connect(const char *name, signal_callback_t callback)
{
    struct cwc_signal_entry *sig_entry =
        get_signal_entry_or_create_if_not_exist(name);

    struct signal_c_callback *c_callback = malloc(sizeof(*c_callback));
    c_callback->callback                 = callback;
    wl_list_insert(sig_entry->c_callbacks.prev, &c_callback->link);
}

void cwc_signal_connect_lua(const char *name, lua_State *L, int n)
{
    struct cwc_signal_entry *sig_entry =
        get_signal_entry_or_create_if_not_exist(name);

    struct signal_lua_callback *lua_callback = malloc(sizeof(*lua_callback));
    wl_list_insert(sig_entry->lua_callbacks.prev, &lua_callback->link);

    lua_pushvalue(L, n);
    lua_callback->luaref = luaL_ref(L, LUA_REGISTRYINDEX);
}

static inline void signal_c_callback_destroy(struct signal_c_callback *c_cb)
{
    wl_list_remove(&c_cb->link);
    free(c_cb);
}

void cwc_signal_disconnect(const char *name, signal_callback_t callback)
{
    struct cwc_signal_entry *sig_entry =
        get_signal_entry_or_create_if_not_exist(name);

    struct signal_c_callback *c_callback;
    wl_list_for_each_reverse(c_callback, &sig_entry->c_callbacks, link)
    {
        if (c_callback->callback == callback) {
            signal_c_callback_destroy(c_callback);
            return;
        }
    }
}

static inline void signal_lua_callback_destroy(lua_State *L,
                                               struct signal_lua_callback *l_cb)
{
    luaL_unref(L, LUA_REGISTRYINDEX, l_cb->luaref);
    wl_list_remove(&l_cb->link);
    free(l_cb);
}

void cwc_signal_disconnect_lua(const char *name, lua_State *L, int idx)
{
    struct cwc_signal_entry *sig_entry =
        get_signal_entry_or_create_if_not_exist(name);

    struct signal_lua_callback *lua_callback;
    wl_list_for_each_reverse(lua_callback, &sig_entry->lua_callbacks, link)
    {
        lua_pushvalue(L, idx);
        lua_rawgeti(L, LUA_REGISTRYINDEX, lua_callback->luaref);

        bool equal = lua_rawequal(L, -1, -2);
        lua_pop(L, 2);

        if (equal) {
            signal_lua_callback_destroy(L, lua_callback);
            return;
        }
    }
}

static void signal_entry_wipe_lua(struct cwc_signal_entry *sig_entry)
{
    struct signal_lua_callback *cb;
    struct signal_lua_callback *tmp;
    wl_list_for_each_safe(cb, tmp, &sig_entry->lua_callbacks, link)
    {
        signal_lua_callback_destroy(g_config_get_lua_State(), cb);
    }
}

void cwc_lua_signal_clear(struct cwc_hhmap *map)
{
    for (uint64_t i = 0; i < map->alloc; i++) {
        struct hhash_entry *elem = &map->table[i];
        if (!elem->hash)
            continue;

        struct cwc_signal_entry *sig_entry = elem->data;
        signal_entry_wipe_lua(sig_entry);
    }
}

static void _emit_c(struct cwc_signal_entry *sig_entry, void *data)
{
    struct signal_c_callback *c_callback;
    wl_list_for_each(c_callback, &sig_entry->c_callbacks, link)
    {
        c_callback->callback(data);
    }
}

static void
_emit_lua(struct cwc_signal_entry *sig_entry, lua_State *L, int nargs)
{
    int initial_stack_size = lua_gettop(L);

    struct signal_lua_callback *lua_callback;
    wl_list_for_each(lua_callback, &sig_entry->lua_callbacks, link)
    {
        // push function and the argument
        lua_rawgeti(L, LUA_REGISTRYINDEX, lua_callback->luaref);
        for (int i = nargs; i > 0; i--) {
            lua_pushvalue(L, initial_stack_size - i + 1);
        }

        if (lua_pcall(L, nargs, 0, 0)) {
            cwc_log(CWC_ERROR, "error when executing lua function: %s",
                    lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
}

void cwc_signal_emit_c(const char *name, void *data)
{
    struct cwc_signal_entry *sig_entry =
        get_signal_entry_or_create_if_not_exist(name);

    _emit_c(sig_entry, data);
}

void cwc_signal_emit_lua(const char *name, lua_State *L, int nargs)
{
    struct cwc_signal_entry *sig_entry =
        get_signal_entry_or_create_if_not_exist(name);

    _emit_lua(sig_entry, L, nargs);
}

void cwc_signal_emit(const char *name, void *data, lua_State *L, int nargs)
{
    struct cwc_signal_entry *sig_entry = cwc_hhmap_get(server.signal_map, name);
    if (!sig_entry)
        return;

    _emit_c(sig_entry, data);
    _emit_lua(sig_entry, L, nargs);
}

void cwc_object_emit_signal_varr(const char *name, lua_State *L, int nargs, ...)
{
    va_list argptr;
    va_start(argptr, nargs);

    void **ptr_list[nargs + 1];
    ptr_list[nargs] = NULL;

    for (int i = 0; i < nargs; i++) {
        void *data  = va_arg(argptr, void *);
        ptr_list[i] = data;
        luaC_object_push(L, data);
    }

    cwc_signal_emit(name, ptr_list, L, nargs);

    va_end(argptr);
}
