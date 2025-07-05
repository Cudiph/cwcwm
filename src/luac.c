/* luac.c - cwc lua C library
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

/** cwc lifecycle and low-level APIs.
 *
 * @author Dwi Asmoro Bangun
 * @copyright 2024
 * @license GPLv3
 * @coreclassmod cwc
 */

#include <lauxlib.h>
#include <libgen.h>
#include <lua.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/backend/drm.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/wayland.h>
#include <wlr/backend/x11.h>

#include "cwc/config.h"
#include "cwc/desktop/layer_shell.h"
#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/input/cursor.h"
#include "cwc/input/keyboard.h"
#include "cwc/input/manager.h"
#include "cwc/luac.h"
#include "cwc/luaclass.h"
#include "cwc/plugin.h"
#include "cwc/server.h"
#include "cwc/signal.h"
#include "cwc/timer.h"
#include "cwc/util.h"
#include "private/luac.h"

/** Quit cwc.
 * @staticfct quit
 * @noreturn
 */
static int luaC_quit(lua_State *L)
{
    wl_display_terminate(server.wl_display);
    return 0;
}

/* all the registered object is lost when reloaded since we closing the lua
 * state, register again here.
 */
static void reregister_lua_object()
{
    lua_State *L = g_config_get_lua_State();

    struct cwc_toplevel *toplevel;
    wl_list_for_each(toplevel, &server.toplevels, link)
    {
        luaC_object_client_register(L, toplevel);
        cwc_object_emit_signal_simple("client::new", L, toplevel);
    }

    struct cwc_container *container;
    wl_list_for_each(container, &server.containers, link)
    {
        luaC_object_container_register(L, container);
        cwc_object_emit_signal_simple("container::new", L, container);
    }

    struct cwc_output *output;
    wl_list_for_each(output, &server.outputs, link)
    {
        // register tag first because screen dependent on it
        for (int i = 0; i < MAX_WORKSPACE; i++) {
            luaC_object_tag_register(L, &output->state->tag_info[i]);
        }

        luaC_object_screen_register(L, output);
        cwc_object_emit_signal_simple("screen::new", g_config_get_lua_State(),
                                      output);
    }

    struct cwc_libinput_device *input_dev;
    wl_list_for_each(input_dev, &server.input->devices, link)
    {
        luaC_object_input_register(L, input_dev);
        cwc_object_emit_signal_simple("input::new", L, input_dev);
    }

    struct cwc_layer_surface *lsurf;
    wl_list_for_each(lsurf, &server.layer_shells, link)
    {
        luaC_object_layer_shell_register(L, lsurf);
        cwc_object_emit_signal_simple("layer_shell::new", L, lsurf);
    }
}

/* Reloading the lua configuration is kinda unfun because we safe some lua value
 * and object and need to keep track of it. Here is the list value that saved
 * from the lua registry that need to be cleared form the C data:
 *
 * - keyboard binding
 * - mouse binding
 * - lua signal
 */
static void cwc_restart_lua(void *data)
{
    cwc_log(CWC_INFO, "reloading configuration...");

    struct cwc_keybind_map *kmap, *tmp;
    wl_list_for_each_safe(kmap, tmp, &server.kbd_kmaps, link)
    {
        cwc_keybind_map_destroy(kmap);
    }
    cwc_keybind_map_destroy(server.main_kbd_kmap);
    server.main_kbd_kmap = cwc_keybind_map_create(NULL);
    cwc_keybind_map_clear(server.main_mouse_kmap);

    struct cwc_timer *timer, *timer_tmp;
    wl_list_for_each_safe(timer, timer_tmp, &server.timers, link)
    {
        cwc_timer_destroy(timer);
    }

    cwc_lua_signal_clear(server.signal_map);
    luaC_fini();

    /****************** OLD -> NEW STATE BARRIER ********************/

    luaC_init();
    reregister_lua_object();
    cwc_signal_emit_c("lua::reload", NULL);
    cwc_config_commit();
}

/** Reload cwc lua configuration.
 * @staticfct reload
 * @noreturn
 */
static int luaC_reload(lua_State *L)
{
    // there is unfortunately no article about restarting lua state inside a lua
    // C function, pls someone tell me if there is a better way.
    wl_event_loop_add_idle(server.wl_event_loop, cwc_restart_lua, NULL);
    return 0;
}

/** Commit configuration change.
 * @staticfct commit
 * @noreturn
 */
static int luaC_commit(lua_State *L)
{
    cwc_config_commit();
    return 0;
}

/** Spawn program.
 * @staticfct spawn
 * @tparam string[] vargs Array of argument list
 * @noreturn
 */
static int luaC_spawn(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    int len = lua_objlen(L, 1);

    char *argv[len + 1];
    argv[len] = NULL;
    int i;
    for (i = 0; i < len; ++i) {
        lua_rawgeti(L, 1, i + 1);
        if (!lua_isstring(L, -1))
            goto cleanup;

        argv[i] = strdup(lua_tostring(L, -1));
    }

    spawn(argv);

cleanup:
    for (int j = 0; j < i; ++j)
        free(argv[j]);

    if (i != len)
        luaL_error(L, "Expected array of string");

    return 0;
}

/** Spawn program with shell.
 * @staticfct spawn_with_shell
 * @tparam string cmd Shell command
 * @noreturn
 */
static int luaC_spawn_with_shell(lua_State *L)
{
    const char *cmd = luaL_checkstring(L, 1);
    spawn_with_shell(cmd);

    return 0;
}

static void _backend_multi_check_nested(struct wlr_backend *_backend,
                                        void *data)
{
    bool *is_nested = data;
    if (wlr_backend_is_x11(_backend) || wlr_backend_is_wl(_backend))
        *is_nested = true;
}

/** Check if the session is nested.
 * @staticfct is_nested
 * @treturn boolean
 */
static int luaC_is_nested(lua_State *L)
{
    bool returnval = false;

    if (wlr_backend_is_multi(server.backend)) {
        wlr_multi_for_each_backend(server.backend, _backend_multi_check_nested,
                                   &returnval);
    }

    if (wlr_backend_is_drm(server.backend))
        returnval = false;

    if (wlr_backend_is_x11(server.backend) || wlr_backend_is_wl(server.backend))
        returnval = true;

    lua_pushboolean(L, returnval);
    return 1;
}

/** Check if the configuration is startup (not reload).
 * @staticfct is_startup
 * @treturn boolean
 */
static int luaC_is_startup(lua_State *L)
{
    lua_pushboolean(L, lua_initial_load);
    return 1;
}

/** Get cwc datadir location, it will search through `$XDG_DATA_DIRS/share/cwc`.
 * @tfield string datadir
 * @readonly
 */
static int luaC_get_datadir(lua_State *L)
{
    char cwc_datadir[4000];
    get_cwc_datadir(cwc_datadir, 4000);
    lua_pushstring(L, cwc_datadir);
    return 1;
}

/** Get cwc version.
 * @tfield string version
 * @readonly
 */
static int luaC_get_version(lua_State *L)
{
#ifdef CWC_GITHASH
    lua_pushstring(L, CWC_VERSION "-" CWC_GITHASH);
#else
    lua_pushstring(L, CWC_VERSION);
#endif /* ifdef CWC_GITHASH */

    return 1;
}

/** Wrapper of C setenv.
 * @staticfct setenv
 * @tparam string key Variable name.
 * @tparam string val Value.
 * @noreturn
 */
static int luaC_setenv(lua_State *L)
{
    const char *key = luaL_checkstring(L, 1);
    const char *val = luaL_checkstring(L, 2);

    setenv(key, val, 1);
    return 0;
}

/** Wrapper of C unsetenv.
 * @staticfct unsetenv
 * @tparam string key Variable name.
 * @noreturn
 */
static int luaC_unsetenv(lua_State *L)
{
    const char *key = luaL_checkstring(L, 1);

    unsetenv(key);
    return 0;
}

/** Change the vt (chvt).
 * @staticfct chvt
 * @tparam integer n Index of the vt.
 * @noreturn
 */
static int luaC_chvt(lua_State *L)
{
    int vtnum = luaL_checkint(L, 1);

    wlr_session_change_vt(server.session, vtnum);
    return 0;
}

/** Add event listener.
 *
 * @staticfct connect_signal
 * @tparam string signame The name of the signal.
 * @tparam function func Callback function to run.
 * @noreturn
 */
static int luaC_connect_signal(lua_State *L)
{
    luaL_checktype(L, 2, LUA_TFUNCTION);

    const char *name = luaL_checkstring(L, 1);
    cwc_signal_connect_lua(name, L, 2);

    return 0;
}

/** Remove event listener.
 *
 * @staticfct disconnect_signal
 * @tparam string signame The name of the signal.
 * @tparam function func Attached callback function .
 * @noreturn
 */
static int luaC_disconnect_signal(lua_State *L)
{
    luaL_checktype(L, 2, LUA_TFUNCTION);

    const char *name = luaL_checkstring(L, 1);
    cwc_signal_disconnect_lua(name, L, 2);

    return 0;
}

/** Notify event listener.
 *
 * @staticfct emit_signal
 * @tparam string signame The name of the signal.
 * @param ... The signal callback argument.
 * @noreturn
 */
static int luaC_emit_signal(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    int arglen       = lua_gettop(L) - 1;

    cwc_signal_emit_lua(name, L, arglen);

    return 0;
}

static void create_output(struct wlr_backend *backend, void *data)
{
    int *total_output = data;

    if (wl_list_length(&server.outputs) >= *total_output)
        return;

    if (wlr_backend_is_wl(backend)) {
        wlr_wl_output_create(backend);
    } else if (wlr_backend_is_headless(backend)) {
        wlr_headless_add_output(backend, 1920, 1080);
    } else if (wlr_backend_is_x11(backend)) {
        wlr_x11_output_create(backend);
    }
}

/* Create new output.
 *
 * @function create_output
 * @tparam integer total_output
 * @noreturn
 */
static int luaC_create_output(lua_State *L)
{
    int total_output = luaL_checkint(L, 1);

    wlr_multi_for_each_backend(server.backend, create_output, &total_output);

    return 0;
}

/** Set to true to show all clients or false to show clients that on the current
 * tag.
 *
 * only affect widget or bar that implement wlr foreign toplevel management.
 *
 * @tfield[opt=true] boolean tasklist_show_all
 */
static int luaC_get_tasklist_show_all(lua_State *L)
{
    lua_pushboolean(L, g_config.tasklist_show_all);
    return 1;
}
static int luaC_set_tasklist_show_all(lua_State *L)
{
    bool set                   = lua_toboolean(L, 1);
    g_config.tasklist_show_all = set;

    struct cwc_output *o;
    wl_list_for_each(o, &server.outputs, link)
    {
        cwc_output_update_visible(o);
    }

    return 0;
}

/* free it after use */
static char *get_xdg_config_home()
{
    char *config_home_env = getenv("XDG_CONFIG_HOME");
    if (config_home_env == NULL) {
        const char *HOME          = getenv("HOME");
        const char *config_folder = "/.config";
        config_home_env = calloc(strlen(HOME) + strlen(config_folder) + 1,
                                 sizeof(*config_home_env));
        strcpy(config_home_env, HOME);
        strcat(config_home_env, config_folder);
        return config_home_env;
    }

    char *temp = calloc(strlen(config_home_env) + 1, sizeof(*temp));
    strcpy(temp, config_home_env);

    return temp;
}

/* free it after use */
static char *get_luarc_path()
{
    char *config_home   = get_xdg_config_home();
    char *init_filepath = "/cwc/rc.lua";
    char *temp =
        calloc(strlen(config_home) + strlen(init_filepath) + 1, sizeof(*temp));
    strcpy(temp, config_home);
    strcat(temp, init_filepath);

    free(config_home);
    return temp;
}

static void add_to_search_path(lua_State *L, char *_dirname)
{
    lua_getglobal(L, "package");

    // package.path += ";" .. _dirname .. "/?.lua"
    lua_getfield(L, -1, "path");
    lua_pushstring(L, ";");
    lua_pushstring(L, _dirname);
    lua_pushstring(L, "/?.lua");
    lua_concat(L, 4);
    lua_setfield(L, -2, "path");

    //  package.path += ";" .. _dirname .. "/?/init.lua"
    lua_getfield(L, -1, "path");
    lua_pushstring(L, ";");
    lua_pushstring(L, _dirname);
    lua_pushstring(L, "/?/init.lua");
    lua_concat(L, 4);
    lua_setfield(L, -2, "path");
}

/* return 0 if success */
static int luaC_loadrc(lua_State *L, char *path)
{
    char *dir = dirname(strdup(path));
    add_to_search_path(L, dir);
    free(dir);

    if (luacheck)
        printf("Checking config '%s'...", path);

    if (luaL_dofile(L, path)) {
        if (luacheck)
            printf("\nERROR: %s\n", lua_tostring(L, -1));
        cwc_log(CWC_ERROR, "cannot run configuration file: %s",
                lua_tostring(L, -1));
        return 1;
    }

    if (luacheck)
        puts(" OK");

    return 0;
}

#define TABLE_RO(name)     {"get_" #name, luaC_get_##name}
#define TABLE_SETTER(name) {"set_" #name, luaC_set_##name}
#define TABLE_FIELD(name)  TABLE_RO(name), TABLE_SETTER(name)

/* lua stuff start here */
int luaC_init()
{
    struct lua_State *L = g_config._L_but_better_to_use_function_than_directly =
        luaL_newstate();
    luaL_openlibs(L);

    // get datadir path
    char cwc_datadir[4096];
    get_cwc_datadir(cwc_datadir, 4000);
    int datadir_len = strlen(cwc_datadir);
    strcat(cwc_datadir, "/lib");

    add_to_search_path(L, library_path ? library_path : cwc_datadir);
    cwc_datadir[datadir_len] = 0;

    // awesome compability for awesome module
    cwc_assert(
        !luaL_dostring(L, "awesome = { connect_signal = function() end}"),
        "incorrect dostring");
    lua_settop(L, 0);

    // reg c lib
    luaL_Reg cwc_lib[] = {
        {"quit",              luaC_quit             },
        {"reload",            luaC_reload           },
        {"commit",            luaC_commit           },
        {"spawn",             luaC_spawn            },
        {"spawn_with_shell",  luaC_spawn_with_shell },
        {"setenv",            luaC_setenv           },
        {"unsetenv",          luaC_unsetenv         },
        {"chvt",              luaC_chvt             },

        {"connect_signal",    luaC_connect_signal   },
        {"disconnect_signal", luaC_disconnect_signal},
        {"emit_signal",       luaC_emit_signal      },

        {"is_nested",         luaC_is_nested        },
        {"is_startup",        luaC_is_startup       },
        TABLE_RO(datadir),
        TABLE_RO(version),

        // config functions
        TABLE_FIELD(tasklist_show_all),

        // intended for dev use only
        {"create_output",     luaC_create_output    },

        {NULL,                NULL                  },
    };

    /* all the setup function will use the cwc table on top of the stack and
     * keep the stack original.
     */
    luaC_register_table(L, "cwc", cwc_lib, NULL);
    lua_setglobal(L, "cwc");
    lua_getglobal(L, "cwc");

    /* setup lua object registry table */
    luaC_object_setup(L);

    /* cwc.client */
    luaC_client_setup(L);

    /* cwc.container */
    luaC_container_setup(L);

    /* cwc.kbd */
    luaC_kbd_setup(L);

    /* cwc.mouse */
    luaC_pointer_setup(L);

    /* cwc.screen */
    luaC_screen_setup(L);

    /* cwc.plugin */
    luaC_plugin_setup(L);

    /* cwc.input */
    luaC_input_setup(L);

    /* cwc.layer_shell */
    luaC_layer_shell_setup(L);

    /* cwc_timer */
    luaC_timer_setup(L);

    /* cwc_kbindmap */
    luaC_kbindmap_setup(L);

    /* cwc_kbind */
    luaC_kbind_setup(L);

    /* cwc_tag */
    luaC_tag_setup(L);

    strcat(cwc_datadir, "/defconfig/rc.lua");
    char *luarc_default_location = get_luarc_path();
    int has_error                = 0;
    if (config_path && access(config_path, R_OK) == 0) {
        has_error = luaC_loadrc(L, config_path);
    } else if (access(luarc_default_location, R_OK) == 0) {
        if ((has_error = luaC_loadrc(L, luarc_default_location))) {
            cwc_log(CWC_ERROR, "falling back to default configuration");
            has_error = luaC_loadrc(L, cwc_datadir);
        }
    } else {
        cwc_log(CWC_ERROR,
                "lua configuration not found, try create one at \"%s\"",
                luarc_default_location);
        has_error = luaC_loadrc(L, cwc_datadir);
    }

    lua_initial_load = false;
    lua_settop(L, 0);
    free(luarc_default_location);
    return has_error;
}

void luaC_fini()
{
    lua_State *L = g_config_get_lua_State();
    lua_close(L);
    g_config._L_but_better_to_use_function_than_directly = NULL;
}
