/* plugin.c - plugin management
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

/** C plugins lifecycle and management API.
 *
 * @author Dwi Asmoro Bangun
 * @copyright 2024
 * @license GPLv3
 * @coreclassmod cwc.plugin
 */

#include <dlfcn.h>
#include <lauxlib.h>
#include <lua.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-util.h>

#include "cwc/config.h"
#include "cwc/luaclass.h"
#include "cwc/luaobject.h"
#include "cwc/plugin.h"
#include "cwc/server.h"
#include "cwc/signal.h"
#include "cwc/util.h"

/** Emitted when a plugin is loaded.
 *
 * @signal plugin::load
 * @tparam cwc_plugin p The plugin object.
 */

/** Emitted when the plugin is about to be unloaded.
 *
 * @signal plugin::unload
 * @tparam cwc_plugin p The plugin object.
 */

struct cwc_plugin *load_plugin(const char *pathname)
{
    return __load_plugin(pathname, RTLD_LAZY | RTLD_LOCAL);
}

struct cwc_plugin *load_plugin_library(const char *pathname)
{
    return __load_plugin(pathname, RTLD_LAZY | RTLD_GLOBAL);
}

struct cwc_plugin *__load_plugin(const char *pathname, int __mode)
{
    void *handle = dlopen(pathname, __mode);
    if (handle == NULL) {
        cwc_log(CWC_ERROR, "Plugin %s cannot be loaded: %s", pathname,
                dlerror());
        return NULL;
    }

    initcall_t init_fn = dlsym(handle, "__cwc_init_plugin");
    if (init_fn == NULL)
        init_fn = dlsym(handle, "__cwc_init_plugin_global");

    if (init_fn == NULL)
        return NULL;

    struct cwc_plugin *p = calloc(1, sizeof(*p));
    if (p == NULL) {
        cwc_log(CWC_ERROR, "Failed to allocate new cwc_plugin");
        return NULL;
    }

    p->name    = dlsym(handle, PLUGIN_TAG_SYMBOL(name));
    p->version = dlsym(handle, PLUGIN_TAG_SYMBOL(version));
    if (p->name == NULL || p->version == NULL) {
        cwc_log(CWC_ERROR,
                "Plugin %s doesn't define information of PLUGIN_NAME or "
                "PLUGIN_VERSION",
                pathname);
        goto cancel;
    }

    if (cwc_plugin_is_exist(p->name))
        goto cancel;

    p->filename    = strdup(pathname);
    p->description = dlsym(handle, PLUGIN_TAG_SYMBOL(description));
    p->author      = dlsym(handle, PLUGIN_TAG_SYMBOL(author));
    p->license     = dlsym(handle, PLUGIN_TAG_SYMBOL(license));
    p->handle      = handle;
    p->init_fn     = init_fn;

    lua_State *L = g_config_get_lua_State();
    luaC_object_plugin_register(L, p);
    cwc_object_emit_signal_simple("plugin::load", L, p);

    cwc_log(CWC_DEBUG, "loaded plugin: %s", p->name);
    return p;

cancel:
    free(p);
    dlclose(handle);
    return NULL;
}

void cwc_plugin_start(struct cwc_plugin *plugin)
{
    // run in another thread?
    wl_list_insert(&server.plugins, &plugin->link);

    plugin->init_fn();
}

void cwc_plugin_unload(struct cwc_plugin *plugin)
{
    exitcall_t exit_fn = dlsym(plugin->handle, "__cwc_cleanup_plugin");

    // if exit_fn is null then the plugin doesn't support unloading
    if (exit_fn == NULL)
        return;

    lua_State *L = g_config_get_lua_State();
    cwc_object_emit_signal_simple("plugin::unload", L, plugin);

    cwc_log(CWC_DEBUG, "unloading plugin: %s", plugin->name);
    exit_fn();
    dlclose(plugin->handle);

    luaC_object_unregister(L, plugin);
    wl_list_remove(&plugin->link);
    free(plugin->filename);
    free(plugin);
}

bool cwc_plugin_is_exist(const char *name)
{
    struct cwc_plugin *p;
    wl_list_for_each(p, &server.plugins, link)
    {
        if (strcmp(p->name, name) == 0)
            return true;
    }

    return false;
}

bool cwc_plugin_stop_by_name(const char *name)
{
    struct cwc_plugin *p;
    wl_list_for_each(p, &server.plugins, link)
    {
        // PLUGIN_TAG_SYMBOL format is key=val format like  "name=plugname" so
        // start comparing from index 5
        if (strcmp(&p->name[5], name) != 0)
            continue;

        cwc_plugin_unload(p);
        return true;
    }

    return false;
}

void cwc_plugin_stop_plugins(struct wl_list *head)
{
    struct cwc_plugin *p;
    struct cwc_plugin *tmp;
    wl_list_for_each_safe(p, tmp, head, link)
    {
        cwc_plugin_unload(p);
    }
}

//================ LUA ===================

/** Get all loaded plugin into a table.
 *
 * @staticfct get
 * @treturn cwc_plugin[]
 */
static int luaC_plugin_get(lua_State *L)
{
    lua_newtable(L);

    struct cwc_plugin *plugin;
    int i = 1;
    wl_list_for_each(plugin, &server.plugins, link)
    {
        luaC_object_push(L, plugin);
        lua_rawseti(L, -2, i++);
    }

    return 1;
}

/** load C plugin
 *
 * @staticfct load
 * @tparam string path File location of the C shared object
 * @treturn boolean true if loading success
 */
static int luaC_plugin_load(lua_State *L)
{
    const char *path     = luaL_checkstring(L, 1);
    struct cwc_plugin *p = load_plugin(path);

    if (p) {
        cwc_plugin_start(p);
        lua_pushboolean(L, true);
    } else {
        lua_pushboolean(L, false);
    }

    return 1;
}

/** unload C plugin filtered by name
 *
 * @staticfct unload_byname
 * @tparam string name Plugin name
 * @treturn boolean true if unloading success
 */
static int luaC_plugin_unload_byname(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    lua_pushboolean(L, cwc_plugin_stop_by_name(name));

    return 1;
}

/** Unload this plugin.
 *
 * @method unload
 * @noreturn
 */
static int luaC_plugin_unload(lua_State *L)
{
    struct cwc_plugin *plugin = luaC_plugin_checkudata(L, 1);

    cwc_plugin_unload(plugin);

    return 0;
}

#define CWC_PLUGIN_RO_PROPERTY(fieldname)                         \
    static int luaC_plugin_get_##fieldname(lua_State *L)          \
    {                                                             \
        struct cwc_plugin *plugin = luaC_plugin_checkudata(L, 1); \
        if (!plugin->fieldname) {                                 \
            lua_pushstring(L, "");                                \
            return 1;                                             \
        }                                                         \
                                                                  \
        char *value_start = strchr(plugin->fieldname, '=');       \
        value_start       = value_start ? value_start + 1 : NULL; \
        lua_pushstring(L, value_start);                           \
        return 1;                                                 \
    }

/** The name of the plugin.
 *
 * @property name
 * @tparam[opt=''] string name
 * @readonly
 */
CWC_PLUGIN_RO_PROPERTY(name)

/** The description of the plugin.
 *
 * @property description
 * @tparam[opt=''] string description
 * @readonly
 */
CWC_PLUGIN_RO_PROPERTY(description)

/** The version of the plugin.
 *
 * @property version
 * @tparam[opt=''] string version
 * @readonly
 */
CWC_PLUGIN_RO_PROPERTY(version)

/** The author of the plugin.
 *
 * @property author
 * @tparam[opt=''] string author
 * @readonly
 */
CWC_PLUGIN_RO_PROPERTY(author)

/** The license of the plugin.
 *
 * @property license
 * @tparam[opt=''] string license
 * @readonly
 */
CWC_PLUGIN_RO_PROPERTY(license)

/** The filename of the plugin.
 *
 * @property filename
 * @tparam[opt=''] string filename
 * @readonly
 */
static int luaC_plugin_get_filename(lua_State *L)
{
    struct cwc_plugin *plugin = luaC_plugin_checkudata(L, 1);
    lua_pushstring(L, plugin->filename);
    return 1;
}

#define REG_METHOD(name)    {#name, luaC_plugin_##name}
#define REG_READ_ONLY(name) {"get_" #name, luaC_plugin_get_##name}

void luaC_plugin_setup(lua_State *L)
{
    luaL_Reg plugin_metamethods[] = {
        {"__eq",       luaC_plugin_eq      },
        {"__tostring", luaC_plugin_tostring},
        {NULL,         NULL                },
    };

    luaL_Reg plugin_methods[] = {
        REG_METHOD(unload),

        REG_READ_ONLY(name),    REG_READ_ONLY(description),
        REG_READ_ONLY(version), REG_READ_ONLY(author),
        REG_READ_ONLY(license), REG_READ_ONLY(filename),

        {NULL, NULL},
    };

    luaC_register_class(L, plugin_classname, plugin_methods,
                        plugin_metamethods);

    luaL_Reg plugin_staticlibs[] = {
        {"get",           luaC_plugin_get          },
        {"load",          luaC_plugin_load         },
        {"unload_byname", luaC_plugin_unload_byname},
        {NULL,            NULL                     },
    };

    luaC_register_table(L, "cwc.plugin", plugin_staticlibs, NULL);
    lua_setfield(L, -2, "plugin");
}
