/* tablet.c - lua cwc_tablet object
 *
 * Copyright (C) 2026 Dwi Asmoro Bangun <dwiaceromo@gmail.com>
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

/** Tag object for drawing tablet device.
 *
 * @author Dwi Asmoro Bangun
 * @copyright 2026
 * @license GPLv3
 * @inputmodule cwc.tablet
 */

#include <lauxlib.h>
#include <lua.h>
#include <wayland-util.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_ext_workspace_v1.h>
#include <wlr/types/wlr_tablet_v2.h>

#include "cwc/desktop/output.h"
#include "cwc/input/cursor.h"
#include "cwc/input/manager.h"
#include "cwc/input/seat.h"
#include "cwc/input/tablet.h"
#include "cwc/luac.h"
#include "cwc/luaclass.h"
#include "cwc/server.h"

/** A new tablet has been connected.
 *
 * @signal tablet::new
 * @tparam cwc_tablet tablet The tablet object.
 */

/** A tablet has been disconnected.
 *
 * @signal tablet::destroy
 * @tparam cwc_tablet tablet The tablet object.
 */

//============================ CODE =================================

/** Get all tablet in the server.
 *
 * @staticfct get
 * @treturn cwc_tablet[]
 */
static int luaC_tablet_get(lua_State *L)
{
    lua_newtable(L);

    int i = 1;
    struct cwc_seat *seat;
    wl_list_for_each(seat, &server.input->seats, link)
    {
        struct cwc_tablet *tablet;
        wl_list_for_each(tablet, &seat->tablet_devs, link)
        {
            luaC_object_push(L, tablet);
            lua_rawseti(L, -2, i++);
        }
    }

    return 1;
}

/** Map tablet surface to a screen.
 *
 * @method map_to_output
 * @tparam cwc_output output The output.
 * @noreturn
 */
static int luaC_tablet_map_to_output(lua_State *L)
{
    struct cwc_tablet *tablet = luaC_tablet_checkudata(L, 1);
    struct cwc_output *output = luaC_screen_checkudata(L, 2);

    wlr_cursor_map_input_to_output(tablet->seat->cursor->wlr_cursor,
                                   tablet->tablet_v2->wlr_device,
                                   output->wlr_output);

    return 0;
}

/** Map tablet surface to rectangular region.
 *
 * @method map_to_output
 * @tparam table region
 * @tparam integer region.x
 * @tparam integer region.y
 * @tparam integer region.width
 * @tparam integer region.height
 * @noreturn
 */
static int luaC_tablet_map_to_region(lua_State *L)
{
    struct cwc_tablet *tablet = luaC_tablet_checkudata(L, 1);
    struct wlr_box box        = {0};

    luaC_box_from_table(L, 2, &box);
    wlr_cursor_map_input_to_region(tablet->seat->cursor->wlr_cursor,
                                   tablet->tablet_v2->wlr_device, &box);

    return 0;
}

/** Width of the tablet in mm.
 *
 * @property width
 * @tparam[opt=0] integer width
 * @negativeallowed false
 * @readonly
 */
static int luaC_tablet_get_width(lua_State *L)
{
    struct cwc_tablet *tablet = luaC_tablet_checkudata(L, 1);
    lua_pushnumber(L, tablet->tablet_v2->wlr_tablet->width_mm);

    return 1;
}

/** Height of the tablet in mm.
 *
 * @property height
 * @tparam[opt=0] integer height
 * @negativeallowed false
 * @readonly
 */
static int luaC_tablet_get_height(lua_State *L)
{
    struct cwc_tablet *tablet = luaC_tablet_checkudata(L, 1);
    lua_pushnumber(L, tablet->tablet_v2->wlr_tablet->height_mm);

    return 1;
}

/** USB product ID of the tablet (zero if unset).
 *
 * @property usb_product_id
 * @tparam[opt=0] integer usb_product_id
 * @negativeallowed false
 * @readonly
 */
static int luaC_tablet_get_usb_product_id(lua_State *L)
{
    struct cwc_tablet *tablet = luaC_tablet_checkudata(L, 1);
    lua_pushnumber(L, tablet->tablet_v2->wlr_tablet->usb_product_id);

    return 1;
}

/** USB vendor ID of the tablet (zero if unset).
 *
 * @property usb_vendor_id
 * @negativeallowed false
 * @tparam[opt=0] integer usb_vendor_id
 * @readonly
 */
static int luaC_tablet_get_usb_vendor_id(lua_State *L)
{
    struct cwc_tablet *tablet = luaC_tablet_checkudata(L, 1);
    lua_pushnumber(L, tablet->tablet_v2->wlr_tablet->usb_vendor_id);

    return 1;
}

#define REG_METHOD(name)    {#name, luaC_tablet_##name}
#define REG_READ_ONLY(name) {"get_" #name, luaC_tablet_get_##name}
#define REG_SETTER(name)    {"set_" #name, luaC_tablet_set_##name}
#define REG_PROPERTY(name)  REG_READ_ONLY(name), REG_SETTER(name)

void luaC_tablet_setup(lua_State *L)
{
    luaL_Reg tablet_metamethods[] = {
        {"__eq",       luaC_tablet_eq      },
        {"__tostring", luaC_tablet_tostring},
        {NULL,         NULL                },
    };

    luaL_Reg tablet_methods[] = {
        REG_METHOD(get),
        REG_METHOD(map_to_output),
        REG_METHOD(map_to_region),

        REG_READ_ONLY(data),
        REG_READ_ONLY(width),
        REG_READ_ONLY(height),
        REG_READ_ONLY(usb_product_id),
        REG_READ_ONLY(usb_vendor_id),

        {NULL, NULL},
    };

    luaC_register_class(L, tablet_classname, tablet_methods,
                        tablet_metamethods);

    luaL_Reg tablet_staticlibs[] = {
        {"get", luaC_tablet_get},

        {NULL,  NULL           },
    };

    luaC_register_table(L, "cwc.tablet", tablet_staticlibs, NULL);
    lua_setfield(L, -2, "tablet");
}
