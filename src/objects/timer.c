/* timer.c - lua timer object
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

/** Low-level API to create a timer.
 *
 * @author Dwi Asmoro Bangun
 * @copyright 2025
 * @license GPLv3
 * @coreclassmod cwc.timer
 */

#include <lauxlib.h>
#include <lua.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-util.h>

#include "cwc/config.h"
#include "cwc/desktop/output.h"
#include "cwc/luaclass.h"
#include "cwc/luaobject.h"
#include "cwc/server.h"
#include "cwc/timer.h"
#include "cwc/util.h"

const char *const LUAC_TIMER_REGISTRY_KEY = "cwc.timer.registry";

static inline void luaC_timer_registry_push(lua_State *L)
{
    lua_pushstring(L, LUAC_OBJECT_REGISTRY_KEY);
    lua_rawget(L, LUA_REGISTRYINDEX);
}

void cwc_timer_destroy(struct cwc_timer *timer)
{
    lua_State *L = g_config_get_lua_State();
    wl_list_remove(&timer->link);
    wl_event_source_remove(timer->timer);
    luaC_object_unregister(L, timer);
    luaC_timer_registry_push(L);
    luaL_unref(L, -1, timer->cb_ref);
    luaL_unref(L, -1, timer->data_ref);
    free(timer);
}

static int timer_timed_out(void *data)
{
    struct cwc_timer *timer = data;

    lua_State *L = g_config_get_lua_State();
    luaC_timer_registry_push(L);
    lua_rawgeti(L, -1, timer->cb_ref);
    if (timer->data_ref)
        lua_rawgeti(L, -2, timer->data_ref);
    else
        lua_pushnil(L);

    if (lua_pcall(L, 1, 0, 0))
        cwc_log(CWC_ERROR, "timer callback contains error : %s",
                lua_tostring(L, -1));

    if (timer->one_shot) {
        cwc_timer_destroy(timer);
    } else if (timer->single_shot) {
        timer->started = false;
    } else {
        wl_event_source_timer_update(timer->timer, timer->timeout_ms);
    }

    return 0;
}

/** Create a new timer.
 *
 * @staticfct new
 * @tparam number timeout Timeout in seconds
 * @tparam function callback Callback when the timer ends
 * @tparam[opt] table options Additional settings
 * @tparam[opt=true] boolean options.autostart Immediately start the timer
 * countdown
 * @tparam[opt=false] boolean options.call_now Immediately call the callback
 * function
 * @tparam[opt=false] boolean options.single_shot Run only once then stop
 * @tparam[opt=false] boolean options.one_shot Run only once then destroy
 * @tparam[opt=nil] any data Userdata callback argument.
 * @noreturn
 */
static int luaC_timer_new(lua_State *L)
{
    double timeout = luaL_checknumber(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    bool has_userdata = !lua_isnoneornil(L, 4);

    struct cwc_timer *timer = calloc(1, sizeof(*timer));
    timer->timeout_ms       = timeout * 1000;

    bool autostart = true;
    bool call_now  = false;
    if (lua_istable(L, 3)) {
        lua_getfield(L, 3, "autostart");
        if (lua_isboolean(L, -1))
            autostart = lua_toboolean(L, -1);

        lua_getfield(L, 3, "call_now");
        if (lua_isboolean(L, -1))
            call_now = lua_toboolean(L, -1);

        lua_getfield(L, 3, "single_shot");
        if (lua_isboolean(L, -1))
            timer->single_shot = lua_toboolean(L, -1);

        lua_getfield(L, 3, "one_shot");
        if (lua_isboolean(L, -1))
            timer->one_shot = lua_toboolean(L, -1);

        lua_pop(L, 4);
    }

    wl_list_insert(&server.timers, &timer->link);

    timer->timer =
        wl_event_loop_add_timer(server.wl_event_loop, timer_timed_out, timer);

    luaC_timer_registry_push(L);
    lua_pushvalue(L, 2);
    timer->cb_ref = luaL_ref(L, -2);
    if (has_userdata) {
        lua_pushvalue(L, 4);
        timer->data_ref = luaL_ref(L, -2);
    }
    lua_pop(L, 1);

    luaC_object_timer_register(L, timer);

    if (autostart) {
        timer->started = true;
        wl_event_source_timer_update(timer->timer, timer->timeout_ms);
    }

    if (call_now)
        timer_timed_out(timer);

    luaC_object_push(L, timer);

    return 1;
}

struct delayed_call_data {
    int cb_ref;
    int data_ref;
};

/* keep in mind that it is not canceled and always executed upon config
 * reloading.
 */
static void delayed_call(void *data)
{
    struct delayed_call_data *call_data = data;
    int cb_ref                          = call_data->cb_ref;
    int data_ref                        = call_data->data_ref;

    lua_State *L = g_config_get_lua_State();
    luaC_timer_registry_push(L);
    lua_rawgeti(L, -1, cb_ref);
    if (data_ref)
        lua_rawgeti(L, -2, data_ref);
    else
        lua_pushnil(L);

    if (lua_pcall(L, 1, 0, 0))
        cwc_log(CWC_ERROR, "delayed_call callback contains error : %s",
                lua_tostring(L, -1));

    luaC_timer_registry_push(L);
    luaL_unref(L, -1, cb_ref);
    luaL_unref(L, -1, data_ref);

    lua_settop(L, 0);

    free(call_data);
}

/** Call the given function at the end of wayland event loop.
 *
 * @staticfct delayed_call
 * @tparam function callback
 * @tparam[opt=nil] any data Userdata callback argument
 * @noreturn
 */
static int luaC_timer_delayed_call(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    bool has_userdata = !lua_isnoneornil(L, 2);

    struct delayed_call_data *data = calloc(1, sizeof(*data));
    luaC_timer_registry_push(L);
    lua_pushvalue(L, 1);
    data->cb_ref = luaL_ref(L, -2);
    if (has_userdata) {
        lua_pushvalue(L, 2);
        data->data_ref = luaL_ref(L, -2);
    }
    wl_event_loop_add_idle(server.wl_event_loop, delayed_call, data);

    return 0;
}

/** Whether the timer is currently running or not.
 *
 * @property started
 * @tparam[opt=false] boolean started
 */
static int luaC_timer_get_started(lua_State *L)
{
    struct cwc_timer *timer = luaC_timer_checkudata(L, 1);

    lua_pushboolean(L, timer->started);

    return 1;
}
static int luaC_timer_set_started(lua_State *L)
{
    struct cwc_timer *timer = luaC_timer_checkudata(L, 1);
    bool started            = lua_toboolean(L, 2);

    if (!timer->started && started) {
        wl_event_source_timer_update(timer->timer, timer->timeout_ms);
    } else if (!started) {
        wl_event_source_timer_update(timer->timer, 0);
    }

    timer->started = started;

    return 1;
}

static int luaC_timer_start(lua_State *L);

/** The timer timeout value in seconds.
 *
 * @property timeout
 * @tparam[opt=false] boolean timeout
 */
static int luaC_timer_get_timeout(lua_State *L)
{
    struct cwc_timer *timer = luaC_timer_checkudata(L, 1);

    lua_pushnumber(L, timer->timeout_ms);

    return 1;
}
static int luaC_timer_set_timeout(lua_State *L)
{
    struct cwc_timer *timer = luaC_timer_checkudata(L, 1);
    double timeout          = luaL_checknumber(L, 2);
    timer->timeout_ms       = timeout * 1000;

    if (timer->started)
        wl_event_source_timer_update(timer->timer, timer->timeout_ms);

    return 0;
}

/** Start the timer.
 *
 * @method start
 * @noreturn
 */
static int luaC_timer_start(lua_State *L)
{
    struct cwc_timer *timer = luaC_timer_checkudata(L, 1);

    if (!timer->started)
        wl_event_source_timer_update(timer->timer, timer->timeout_ms);

    return 0;
}

/** Stop the timer.
 *
 * @method stop
 * @noreturn
 */
static int luaC_timer_stop(lua_State *L)
{
    struct cwc_timer *timer = luaC_timer_checkudata(L, 1);

    wl_event_source_timer_update(timer->timer, 0);
    timer->started = false;

    return 0;
}

/** Restart the timer.
 *
 * Equivalent to stopping the timer and then starting it again.
 *
 * @method again
 * @noreturn
 */
static int luaC_timer_again(lua_State *L)
{
    luaC_timer_stop(L);
    luaC_timer_start(L);

    return 0;
}

/** Destroy the timer, invalidate the object and freeing it from memory.
 *
 * @method destroy
 * @noreturn
 */
static int luaC_timer_destroy(lua_State *L)
{
    struct cwc_timer *timer = luaC_timer_checkudata(L, 1);

    cwc_timer_destroy(timer);

    return 0;
}

#define REG_METHOD(name)    {#name, luaC_timer_##name}
#define REG_READ_ONLY(name) {"get_" #name, luaC_timer_get_##name}
#define REG_SETTER(name)    {"set_" #name, luaC_timer_set_##name}
#define REG_PROPERTY(name)  REG_READ_ONLY(name), REG_SETTER(name)

void luaC_timer_setup(lua_State *L)
{
    luaL_Reg timer_metamethods[] = {
        {"__eq",       luaC_timer_eq      },
        {"__tostring", luaC_timer_tostring},
        {NULL,         NULL               },
    };

    luaL_Reg timer_methods[] = {
        REG_METHOD(start),     REG_METHOD(stop),
        REG_METHOD(again),     REG_METHOD(destroy),

        REG_READ_ONLY(data),

        REG_PROPERTY(started), REG_PROPERTY(timeout),

        {NULL, NULL},
    };

    luaC_register_class(L, timer_classname, timer_methods, timer_metamethods);

    luaL_Reg timer_staticlibs[] = {
        {"new",          luaC_timer_new         },
        {"delayed_call", luaC_timer_delayed_call},

        {NULL,           NULL                   },
    };

    luaC_register_table(L, "cwc.timer", timer_staticlibs, NULL);
    lua_setfield(L, -2, "timer");

    /* create a table for storing timer callback */
    lua_pushstring(L, LUAC_TIMER_REGISTRY_KEY);
    lua_newtable(L);
    lua_rawset(L, LUA_REGISTRYINDEX);
}
