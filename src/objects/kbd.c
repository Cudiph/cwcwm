/* kbd.c - lua kbd object
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
#include <lua.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_seat.h>

#include "cwc/config.h"
#include "cwc/input/keyboard.h"
#include "cwc/input/manager.h"
#include "cwc/input/seat.h"
#include "cwc/luaclass.h"
#include "cwc/server.h"
#include "cwc/util.h"

/** Emitted when a key is pressed.
 *
 * @signal kbd::pressed
 * @tparam cwc_kbd kbd The keyboard object.
 * @tparam integer time_msec The event time in milliseconds.
 * @tparam integer keycode The xkb keycode.
 * @tparam string name Name of the key.
 */

/** Emitted when a key is released.
 *
 * @signal kbd::released
 * @tparam cwc_kbd kbd The keyboard object.
 * @tparam integer time_msec The event time in milliseconds.
 * @tparam integer keycode The xkb keycode.
 * @tparam string name Name of the key.
 */
//============================ CODE =================================

/** Get all keyboard in the server.
 *
 * @staticfct get
 * @treturn cwc_kbd[]
 */
static int luaC_kbd_get(lua_State *L)
{
    lua_newtable(L);

    int i = 1;
    struct cwc_seat *seat;
    wl_list_for_each(seat, &server.input->seats, link)
    {
        luaC_object_push(L, seat->kbd_group);
        lua_rawseti(L, -2, i++);
    }

    return 1;
}

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

    int data_index = 4;
    if (on_release_is_function) {
        lua_pushvalue(L, 4);
        info.luaref_release = luaL_ref(L, LUA_REGISTRYINDEX);
        data_index++;
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

        lua_getfield(L, data_index, "pass");
        info.pass = lua_toboolean(L, -1);
    }

    // ready for register
    keybind_register(kmap, modifiers, keysym, info);

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
 * @tparam[opt] boolean data.pass Keypress will still pass through the client
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

static bool xkb_idle_run = false;

void update_xkb_rule(void *data)
{
    cwc_keyboard_update_keymap(
        &server.seat->kbd_group->wlr_kbd_group->keyboard);

    xkb_idle_run = false;
}

void update_xkb_idle()
{
    if (xkb_idle_run)
        return;

    xkb_idle_run = true;
    wl_event_loop_add_idle(server.wl_event_loop, update_xkb_rule, NULL);
}

#define XKB_RULE_FIELD(name)                         \
    static int luaC_kbd_get_xkb_##name(lua_State *L) \
    {                                                \
        lua_pushstring(L, g_config.xkb_##name);      \
        return 1;                                    \
    }                                                \
    static int luaC_kbd_set_xkb_##name(lua_State *L) \
    {                                                \
        const char *set = luaL_checkstring(L, 1);    \
                                                     \
        free(g_config.xkb_##name);                   \
        g_config.xkb_##name = strdup(set);           \
        update_xkb_idle();                           \
        return 0;                                    \
    }

/** The rules file to use.
 *
 * The rules file describes how to interpret the values of the model, layout,
 * variant and options fields.
 *
 * ref: [xkbcommon
 * reference](https://xkbcommon.org/doc/current/structxkb__rule__names.html#a0968f4602001f2306febd32c34bd2280)
 *
 * @tfield string xkb_rules
 */
XKB_RULE_FIELD(rules);

/** The keyboard model by which to interpret keycodes and LEDs.
 *
 * ref: [xkbcommon
 * reference](https://xkbcommon.org/doc/current/structxkb__rule__names.html#a1c897b49b49c7cd495db4a424bd27265)
 *
 * @tfield string xkb_model
 */
XKB_RULE_FIELD(model);

/** A comma separated list of layouts (languages) to include in the keymap.
 *
 * ref: [xkbcommon
 * reference](https://xkbcommon.org/doc/current/structxkb__rule__names.html#a243bb3aa36b4ea9ca402ed330a757746)
 *
 * @tfield string xkb_layout
 */
XKB_RULE_FIELD(layout);

/** A comma separated list of variants.
 *
 * One per layout, which may modify or augment the respective layout in various
 * ways.
 *
 * Generally, should either be empty or have the same number of values as the
 * number of layouts. You may use empty values as in "intl,,neo".
 *
 * ref: [xkbcommon
 * reference](https://xkbcommon.org/doc/current/structxkb__rule__names.html#a758e1865c002d8f4fb59b2e3dda83b66)
 *
 * @tfield string xkb_variant
 */
XKB_RULE_FIELD(variant);

/** A comma separated list of options.
 *
 * Through which the user specifies non-layout related preferences, like which
 * key combinations are used for switching layouts, or which key is the Compose
 * key.
 *
 * ref: [xkbcommon
 * reference](https://xkbcommon.org/doc/current/structxkb__rule__names.html#a556899eafb333c440be64ede408644df)
 *
 * @tfield string xkb_options
 */
XKB_RULE_FIELD(options);

/** The seat names which the keyboard belong.
 *
 * @property seat
 * @tparam[opt="seat0"] string seat
 * @readonly
 */
static int luaC_kbd_get_seat(lua_State *L)
{
    struct cwc_keyboard_group *kbdg = luaC_kbd_checkudata(L, 1);
    lua_pushstring(L, kbdg->seat->wlr_seat->name);
    return 1;
}

/** Get active modifiers in this keyboard.
 *
 * @property modifiers
 * @tparam[opt=0] bitfield modifiers
 * @readonly
 */
static int luaC_kbd_get_modifiers(lua_State *L)
{
    struct cwc_keyboard_group *kbdg = luaC_kbd_checkudata(L, 1);

    uint32_t mods = wlr_keyboard_get_modifiers(&kbdg->wlr_kbd_group->keyboard);
    lua_pushnumber(L, mods);

    return 1;
}

/** Grab the keyboard event and redirect it to signal.
 *
 * @property grab
 * @tparam[opt=false] boolean grab
 * @readonly
 */
static int luaC_kbd_get_grab(lua_State *L)
{
    struct cwc_keyboard_group *kbdg = luaC_kbd_checkudata(L, 1);
    lua_pushboolean(L, kbdg->grab);

    return 1;
}
static int luaC_kbd_set_grab(lua_State *L)
{
    struct cwc_keyboard_group *kbdg = luaC_kbd_checkudata(L, 1);
    bool grab                       = lua_toboolean(L, 2);
    kbdg->grab                      = grab;

    return 0;
}

/** Send keyboard events to the client.
 *
 * @property send_events
 * @tparam[opt=true] boolean send_events
 * @readonly
 */
static int luaC_kbd_get_send_events(lua_State *L)
{
    struct cwc_keyboard_group *kbdg = luaC_kbd_checkudata(L, 1);
    lua_pushboolean(L, kbdg->send_events);

    return 1;
}
static int luaC_kbd_set_send_events(lua_State *L)
{
    struct cwc_keyboard_group *kbdg = luaC_kbd_checkudata(L, 1);
    bool send_events                = lua_toboolean(L, 2);
    kbdg->send_events               = send_events;

    return 0;
}

#define REG_METHOD(name)    {#name, luaC_kbd_##name}
#define REG_READ_ONLY(name) {"get_" #name, luaC_kbd_get_##name}
#define REG_SETTER(name)    {"set_" #name, luaC_kbd_set_##name}
#define REG_PROPERTY(name)  REG_READ_ONLY(name), REG_SETTER(name)

#define FIELD_RO(name)     {"get_" #name, luaC_kbd_get_##name}
#define FIELD_SETTER(name) {"set_" #name, luaC_kbd_set_##name}
#define FIELD(name)        FIELD_RO(name), FIELD_SETTER(name)

void luaC_kbd_setup(lua_State *L)
{

    luaL_Reg kbd_metamethods[] = {
        {"__eq",       luaC_kbd_eq      },
        {"__tostring", luaC_kbd_tostring},
        {NULL,         NULL             },
    };

    luaL_Reg kbd_methods[] = {
        REG_READ_ONLY(data),
        REG_READ_ONLY(seat),
        REG_READ_ONLY(modifiers),

        REG_PROPERTY(grab),
        REG_PROPERTY(send_events),

        {NULL, NULL},
    };

    luaC_register_class(L, kbd_classname, kbd_methods, kbd_metamethods);

    luaL_Reg kbd_staticlibs[] = {
        {"get",            luaC_kbd_get           },

        {"bind",           luaC_kbd_bind          },
        {"clear",          luaC_kbd_clear         },
        {"create_bindmap", luaC_kbd_create_bindmap},

        FIELD_RO(bindmap),
        FIELD_RO(default_member),

        FIELD(repeat_rate),
        FIELD(repeat_delay),

        FIELD(xkb_rules),
        FIELD(xkb_model),
        FIELD(xkb_layout),
        FIELD(xkb_variant),
        FIELD(xkb_options),

        {NULL,             NULL                   },
    };

    luaC_register_table(L, "cwc.kbd", kbd_staticlibs, NULL);
    lua_setfield(L, -2, "kbd");
}
