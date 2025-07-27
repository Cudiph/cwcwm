/* input.c - lua cwc_input object
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

/** Lua library for managing input device from libinput.
 *
 * @author Dwi Asmoro Bangun
 * @copyright 2025
 * @license GPLv3
 * @inputmodule cwc.input
 */

#include <lauxlib.h>
#include <libinput.h>
#include <lua.h>
#include <wayland-util.h>

#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/input/manager.h"
#include "cwc/luaclass.h"
#include "cwc/server.h"

/** Emitted when an input device is created.
 *
 * @signal input::new
 * @tparam cwc_input device The input device object.
 */

/** Emitted when an input device is about to be destroyed.
 *
 * @signal input::destroy
 * @tparam cwc_input device The input device object.
 */

//================== CODE ====================

/** Get all input device in the server.
 *
 * @staticfct get
 * @treturn cwc_input[]
 */
static int luaC_input_get(lua_State *L)
{
    lua_newtable(L);

    int i = 1;
    struct cwc_libinput_device *input_dev;
    wl_list_for_each(input_dev, &server.input->devices, link)
    {
        luaC_object_push(L, input_dev);
        lua_rawseti(L, -2, i++);
    }

    return 1;
}

/** Type of this device.
 *
 * @property type
 * @tparam integer type
 * @readonly
 * @negativeallowed false
 * @see cuteful.enum.device_type
 * @propertydefault provided by wlroots.
 */
static int luaC_input_get_type(lua_State *L)
{
    struct cwc_libinput_device *idev = luaC_input_checkudata(L, 1);

    lua_pushinteger(L, idev->wlr_input_dev->type);

    return 1;
}

/* macro expansion */
// static int luaC_input_get_name(lua_State *L)
// {
//     struct cwc_libinput_device *idev = luaC_input_checkudata(L, 1);
//
//     lua_pushstring(L, libinput_device_get_name(idev->device));
//
//     return 1;
// }

#define LIBINPUT_DEVICE_FORWARD_PROPERTY(propname, datatype)                 \
    static int luaC_input_get_##propname(lua_State *L)                       \
    {                                                                        \
        struct cwc_libinput_device *idev = luaC_input_checkudata(L, 1);      \
        lua_push##datatype(L, libinput_device_get_##propname(idev->device)); \
        return 1;                                                            \
    }

/** The descriptive device name as advertised by the kernel and/or the hardware
 * itself.
 *
 * @property name
 * @tparam string name
 * @readonly
 * @propertydefault provided by libinput.
 */
LIBINPUT_DEVICE_FORWARD_PROPERTY(name, string);

/** System name of this device.
 *
 * @property sysname
 * @tparam string sysname
 * @readonly
 * @propertydefault provided by libinput.
 */
LIBINPUT_DEVICE_FORWARD_PROPERTY(sysname, string);

/** The name of the output this device is mapped to, or nil if no output is set.
 *
 * @property output_name
 * @tparam string output_name
 * @readonly
 * @propertydefault provided by libinput.
 */
LIBINPUT_DEVICE_FORWARD_PROPERTY(output_name, string);

/** The vendor ID of this device.
 *
 * @property id_vendor
 * @tparam integer id_vendor
 * @readonly
 * @negativeallowed false
 * @propertydefault provided by libinput.
 */
LIBINPUT_DEVICE_FORWARD_PROPERTY(id_vendor, number);

/** The bus type ID of this device.
 *
 * @property id_bustype
 * @tparam integer id_bustype
 * @readonly
 * @negativeallowed false
 * @propertydefault provided by libinput.
 */
LIBINPUT_DEVICE_FORWARD_PROPERTY(id_bustype, number);

/** The product ID of this device.
 *
 * @property id_product
 * @tparam integer id_product
 * @readonly
 * @negativeallowed false
 * @propertydefault provided by libinput.
 */
LIBINPUT_DEVICE_FORWARD_PROPERTY(id_product, number);

// static int luaC_input_get_accel_speed(lua_State *L)
// {
//     struct cwc_libinput_device *idev = luaC_input_checkudata(L, 1);
//
//     lua_pushnumber(L, libinput_device_config_accel_get_speed(idev->device));
//
//     return 1;
// }
//
// static int luaC_input_set_accel_speed(lua_State *L)
// {
//     struct cwc_libinput_device *idev = luaC_input_checkudata(L, 1);
//     double speed                     = luaL_checknumber(L, 2);
//
//     libinput_device_config_accel_set_speed(idev->device, speed);
//
//     return 0;
// }

#define LIBINPUT_DEVICE_CREATE_PROPERTY(propname, luatype, ctype, prefix,    \
                                        suffix)                              \
    static int luaC_input_get_##propname(lua_State *L)                       \
    {                                                                        \
        struct cwc_libinput_device *idev = luaC_input_checkudata(L, 1);      \
        lua_push##luatype(                                                   \
            L, libinput_device_config_##prefix##_get##suffix(idev->device)); \
        return 1;                                                            \
    }                                                                        \
    static int luaC_input_set_##propname(lua_State *L)                       \
    {                                                                        \
        struct cwc_libinput_device *idev = luaC_input_checkudata(L, 1);      \
        ctype _arg                       = lua_to##luatype(L, 2);            \
        libinput_device_config_##prefix##_set##suffix(idev->device, _arg);   \
        return 0;                                                            \
    }

/** The send-event mode for this device.
 *
 * Set it to `SEND_EVENTS_DISABLE` to disable the device.
 *
 * @property send_events_mode
 * @tparam integer send_events_mode
 * @negativeallowed false
 * @see cuteful.enum.libinput
 * @propertydefault hardware configuration dependent.
 */
LIBINPUT_DEVICE_CREATE_PROPERTY(
    send_events_mode, integer, int, send_events, _mode);

/** Set the left-handed configuration of the device.
 *
 * @property left_handed
 * @tparam[opt=false] boolean left_handed
 */
LIBINPUT_DEVICE_CREATE_PROPERTY(left_handed, boolean, int, left_handed,
                                /* empty */);

/** Acceleration speed or as a sensitivity when using flat acceleration profile.
 *
 * @property sensitivity
 * @tparam[opt=0.0] number sensitivity
 * @rangestart -1.0
 * @rangestop 1.0
 */
LIBINPUT_DEVICE_CREATE_PROPERTY(sensitivity, number, double, accel, _speed);

/** Acceleration profile of this device.
 *
 * @property accel_profile
 * @tparam integer accel_profile
 * @negativeallowed false
 * @see cuteful.enum.libinput
 * @propertydefault flat profile.
 */
LIBINPUT_DEVICE_CREATE_PROPERTY(accel_profile, integer, int, accel, _profile);

/** Natural scroll property of this device.
 *
 * @property natural_scroll
 * @tparam boolean natural_scroll
 * @propertydefault hardware configuration dependent.
 */
LIBINPUT_DEVICE_CREATE_PROPERTY(
    natural_scroll, boolean, int, scroll, _natural_scroll_enabled);

/** Middle emulation property of this device.
 *
 * @property middle_emulation
 * @tparam boolean middle_emulation
 * @propertydefault hardware configuration dependent.
 */
LIBINPUT_DEVICE_CREATE_PROPERTY(
    middle_emulation, boolean, int, middle_emulation, _enabled);

/** The rotation of a device in degrees clockwise off the logical neutral
 * position.
 *
 * @property rotation_angle
 * @tparam integer rotation_angle
 * @rangestart 0
 * @rangestop 360
 * @see cuteful.enum.libinput
 * @propertydefault hardware configuration dependent.
 */
LIBINPUT_DEVICE_CREATE_PROPERTY(
    rotation_angle, integer, unsigned int, rotation, _angle);

/** Tap-to-click property of this device.
 *
 * @property tap
 * @tparam boolean tap
 * @propertydefault hardware configuration dependent.
 */
LIBINPUT_DEVICE_CREATE_PROPERTY(tap, boolean, int, tap, _enabled);

/** Tap-and-drag property of this device.
 *
 * @property tap_drag
 * @tparam boolean tap_drag
 * @propertydefault hardware configuration dependent.
 */
LIBINPUT_DEVICE_CREATE_PROPERTY(tap_drag, boolean, int, tap, _drag_enabled);

/** Drag-lock during tapping property on this device.
 *
 * @property tap_drag_lock
 * @tparam integer tap_drag_lock
 * @negativeallowed false
 * @propertydefault hardware configuration dependent.
 * @see cuteful.enum.libinput
 */
LIBINPUT_DEVICE_CREATE_PROPERTY(
    tap_drag_lock, integer, int, tap, _drag_lock_enabled);

/** The button click method of this device.
 *
 * @property click_method
 * @tparam integer click_method
 * @negativeallowed false
 * @propertydefault hardware configuration dependent.
 * @see cuteful.enum.libinput
 */
LIBINPUT_DEVICE_CREATE_PROPERTY(click_method, integer, int, click, _method);

/** The scroll method of this device.
 *
 * @property scroll_method
 * @tparam integer scroll_method
 * @negativeallowed false
 * @propertydefault hardware configuration dependent.
 * @see cuteful.enum.libinput
 */
LIBINPUT_DEVICE_CREATE_PROPERTY(scroll_method, integer, int, scroll, _method);

/** Disable-while-typing feature state.
 *
 * @property dwt
 * @tparam boolean dwt
 * @propertydefault hardware configuration dependent.
 * @see cuteful.enum.libinput
 */
LIBINPUT_DEVICE_CREATE_PROPERTY(dwt, boolean, int, dwt, _enabled);

#define REG_READ_ONLY(name) {"get_" #name, luaC_input_get_##name}
#define REG_SETTER(name)    {"set_" #name, luaC_input_set_##name}
#define REG_PROPERTY(name)  REG_READ_ONLY(name), REG_SETTER(name)

void luaC_input_setup(lua_State *L)
{
    luaL_Reg input_metamethods[] = {
        {"__eq",       luaC_input_eq      },
        {"__tostring", luaC_input_tostring},
        {NULL,         NULL               },
    };

    luaL_Reg input_methods[] = {

        // readonly prop
        REG_READ_ONLY(type),
        REG_READ_ONLY(name),
        REG_READ_ONLY(sysname),
        REG_READ_ONLY(output_name),
        REG_READ_ONLY(id_vendor),
        REG_READ_ONLY(id_bustype),
        REG_READ_ONLY(id_product),

        // apply to more than one device type
        REG_PROPERTY(send_events_mode),
        REG_PROPERTY(left_handed),

        // pointer devices
        REG_PROPERTY(sensitivity),
        REG_PROPERTY(accel_profile),
        REG_PROPERTY(natural_scroll),
        REG_PROPERTY(middle_emulation),
        REG_PROPERTY(rotation_angle),

        // touchpad
        REG_PROPERTY(tap),
        REG_PROPERTY(tap_drag),
        REG_PROPERTY(tap_drag_lock),
        REG_PROPERTY(click_method),
        REG_PROPERTY(scroll_method),
        REG_PROPERTY(dwt),

        {NULL, NULL},
    };

    luaC_register_class(L, input_classname, input_methods, input_metamethods);
    luaL_Reg input_staticlibs[] = {
        {"get", luaC_input_get},
        {NULL,  NULL          },
    };

    luaC_register_table(L, "cwc.input", input_staticlibs, NULL);
    lua_setfield(L, -2, "input");
}
