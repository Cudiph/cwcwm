/* screen.c - lua cwc_screen object
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

/** Low-Level API to manage output and the tag/workspace system
 *
 * See also: `cuteful.tag`
 *
 * @author Dwi Asmoro Bangun
 * @copyright 2024
 * @license GPLv3
 * @coreclassmod cwc.screen
 */

#include <lauxlib.h>
#include <lua.h>
#include <wayland-util.h>

#include "cwc/config.h"
#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/layout/container.h"
#include "cwc/luac.h"
#include "cwc/luaclass.h"
#include "cwc/luaobject.h"
#include "cwc/server.h"
#include "cwc/types.h"
#include "cwc/util.h"

/** Emitted when a screen is added.
 *
 * @signal screen::new
 * @tparam cwc_screen s The screen object.
 */

/** Emitted when a screen is about to be removed.
 *
 * @signal screen::destroy
 * @tparam cwc_screen s The screen object.
 */

//============= CODE ================

/** Get all screen in the server.
 *
 * @staticfct get
 * @treturn cwc_screen[]
 */
static int luaC_screen_get(lua_State *L)
{
    lua_newtable(L);

    struct cwc_output *output;
    int i = 1;
    wl_list_for_each(output, &server.outputs, link)
    {
        luaC_object_push(L, output);
        lua_rawseti(L, -2, i++);
    }

    return 1;
}

/** Get current focused screen.
 *
 * @staticfct focused
 * @treturn cwc_screen
 * @noreturn
 */
static int luaC_screen_focused(lua_State *L)
{
    luaC_object_push(L, cwc_output_get_focused());

    return 1;
}

/** Get screen at specified coordinates.
 *
 * @staticfct at
 * @tparam integer x X coordinates
 * @tparam integer y Y coordinates
 * @treturn cwc_screen
 */
static int luaC_screen_at(lua_State *L)
{
    uint32_t x = luaL_checknumber(L, 1);
    uint32_t y = luaL_checknumber(L, 2);

    struct cwc_output *o = cwc_output_at(server.output_layout, x, y);
    if (o)
        lua_pushlightuserdata(L, o);
    else
        lua_pushnil(L);

    return 1;
}

/** Get maximum workspace the compositor can handle.
 *
 * @staticfct get_max_workspace
 * @treturn integer count
 */
static int luaC_screen_get_max_workspace(lua_State *L)
{
    lua_pushnumber(L, MAX_WORKSPACE);

    return 1;
}

/** Set useless gaps width.
 *
 * @configfct set_useless_gaps
 * @noreturn
 */
static int luaC_screen_set_default_useless_gaps(lua_State *L)
{
    int gap_width = luaL_checkint(L, 1);

    cwc_config_set_number_positive(&g_config.useless_gaps, gap_width);

    return 0;
}

/** Toggle tag.
 *
 * @method toggle_tag
 * @tparam integer idx Tag index
 * @noreturn
 */
static int luaC_screen_toggle_tag(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    int tag                   = luaL_checkint(L, 2);

    output->state->active_tag ^= 1 << (tag - 1);
    cwc_output_update_visible(output);
    cwc_output_tiling_layout_update(output, 0);

    return 0;
}

/** Change the layout mode strategy.
 *
 * @method strategy_idx
 * @tparam integer idx
 * @noreturn
 */
static int luaC_screen_strategy_idx(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    int direction             = luaL_checkint(L, 2);

    cwc_output_set_strategy_idx(output, direction);

    return 0;
}

#define SCREEN_PROPERTY_FORWARD_WLR_OUTPUT_PROP(fieldname, datatype) \
    static int luaC_screen_get_##fieldname(lua_State *L)             \
    {                                                                \
        struct cwc_output *output = luaC_screen_checkudata(L, 1);    \
        lua_push##datatype(L, output->wlr_output->fieldname);        \
        return 1;                                                    \
    }

/** The screen width.
 *
 * @property width
 * @tparam integer width
 * @readonly
 * @negativeallowed false
 * @propertydefault Extracted from wlr_output.
 */
SCREEN_PROPERTY_FORWARD_WLR_OUTPUT_PROP(width, integer)

/** The screen height.
 *
 * @property height
 * @tparam integer height
 * @readonly
 * @negativeallowed false
 * @propertydefault Extracted from wlr_output.
 */
SCREEN_PROPERTY_FORWARD_WLR_OUTPUT_PROP(height, integer)

/** The screen refresh rate in mHz (may be zero).
 *
 * @property refresh
 * @tparam integer refresh
 * @readonly
 * @negativeallowed false
 * @propertydefault Extracted from wlr_output.
 */
SCREEN_PROPERTY_FORWARD_WLR_OUTPUT_PROP(refresh, integer)

/** The screen physical width in mm.
 *
 * @property phys_width
 * @tparam integer phys_width
 * @readonly
 * @negativeallowed false
 * @propertydefault Extracted from wlr_output.
 */
SCREEN_PROPERTY_FORWARD_WLR_OUTPUT_PROP(phys_width, integer)

/** The screen physical height in mm.
 *
 * @property phys_height
 * @tparam integer phys_height
 * @readonly
 * @negativeallowed false
 * @propertydefault Extracted from wlr_output.
 */
SCREEN_PROPERTY_FORWARD_WLR_OUTPUT_PROP(phys_height, integer)

/** The screen scale.
 *
 * @property scale
 * @tparam number scale
 * @readonly
 * @negativeallowed false
 * @propertydefault Extracted from wlr_output.
 */
SCREEN_PROPERTY_FORWARD_WLR_OUTPUT_PROP(scale, number)

/** The name of the screen.
 *
 * @property name
 * @tparam string name
 * @readonly
 * @propertydefault screen name extracted from wlr_output.
 */
SCREEN_PROPERTY_FORWARD_WLR_OUTPUT_PROP(name, string)

/** The description of the screen (may be empty).
 *
 * @property description
 * @tparam string description
 * @readonly
 * @propertydefault screen description extracted from wlr_output.
 */
SCREEN_PROPERTY_FORWARD_WLR_OUTPUT_PROP(description, string)

/** The make of the screen (may be empty).
 *
 * @property make
 * @tparam string make
 * @readonly
 * @propertydefault screen make extracted from wlr_output.
 */
SCREEN_PROPERTY_FORWARD_WLR_OUTPUT_PROP(make, string)

/** The model of the screen (may be empty).
 *
 * @property model
 * @tparam string model
 * @readonly
 * @propertydefault screen model extracted from wlr_output.
 */
SCREEN_PROPERTY_FORWARD_WLR_OUTPUT_PROP(model, string)

/** The serial of the screen (may be empty).
 *
 * @property serial
 * @tparam string serial
 * @readonly
 * @propertydefault screen serial extracted from wlr_output.
 */
SCREEN_PROPERTY_FORWARD_WLR_OUTPUT_PROP(serial, string)

/** The screen enabled state.
 *
 * @property enabled
 * @tparam boolean enabled
 * @readonly
 * @propertydefault screen enabled state extracted from wlr_output.
 */
SCREEN_PROPERTY_FORWARD_WLR_OUTPUT_PROP(enabled, boolean)

/** The screen is non desktop or not.
 *
 * @property non_desktop
 * @tparam boolean non_desktop
 * @readonly
 * @propertydefault Extracted from wlr_output.
 */
SCREEN_PROPERTY_FORWARD_WLR_OUTPUT_PROP(non_desktop, boolean)

/** The screen is restored or not.
 *
 * @property restored
 * @tparam[opt=false] boolean restored
 * @readonly
 */
static int luaC_screen_get_restored(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);

    lua_pushboolean(L, output->restored);

    return 1;
}

/** The workarea/usable area of the screen.
 *
 * @property workarea
 * @tparam table workarea
 * @tparam integer workarea.x
 * @tparam integer workarea.y
 * @tparam integer workarea.width
 * @tparam integer workarea.height
 * @readonly
 * @negativeallowed false
 * @propertydefault screen dimension.
 */
static int luaC_screen_get_workarea(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);

    struct wlr_box geom = output->usable_area;

    luaC_pushbox(L, geom);

    return 1;
}

/** The layout mode of the active workspace.
 *
 * @property layout_mode
 * @tparam[opt=0] integer layout_mode Layout mode enum from cuteful.
 * @negativeallowed false
 * @see cuteful.enum.layout_mode
 */
static int luaC_screen_set_layout_mode(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    uint32_t layout_mode      = luaL_checkint(L, 2);

    cwc_output_set_layout_mode(output, layout_mode);

    return 0;
}

static int luaC_screen_get_layout_mode(lua_State *L)
{
    struct cwc_output *output  = luaC_screen_checkudata(L, 1);
    struct cwc_view_info *info = cwc_output_get_current_view_info(output);

    lua_pushinteger(L, info->layout_mode);

    return 1;
}

/** The layout mode of the active workspace.
 *
 * @property allow_tearing
 * @tparam[opt=false] integer layout_mode Layout mode enum from cuteful.
 */
static int luaC_screen_set_allow_tearing(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);

    cwc_output_set_allow_tearing(output, lua_toboolean(L, 2));

    return 0;
}

static int luaC_screen_get_allow_tearing(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);

    lua_pushboolean(L, cwc_output_is_allow_tearing(output));

    return 1;
}

/** Bitfield of currently activated tags.
 *
 * @property active_tag
 * @tparam integer active_tag
 * @negativeallowed false
 * @propertydefault Current active tags.
 *
 */
static int luaC_screen_get_active_tag(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    lua_pushnumber(L, output->state->active_tag);

    return 1;
}

static int luaC_screen_set_active_tag(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);

    tag_bitfield_t newtag     = luaL_checkint(L, 2);
    output->state->active_tag = newtag;

    cwc_output_update_visible(output);

    return 0;
}

/** Currently active workspace.
 *
 * @property active_workspace
 * @tparam integer active_workspace
 * @propertydefault Current active workspace.
 * @negativeallowed false
 */
static int luaC_screen_get_active_workspace(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    lua_pushnumber(L, output->state->active_workspace);

    return 1;
}

/** Does the same as setting active_workspace property.
 *
 * Change output view, for tag it'll reset all the activated tag and activate
 * the specified index, for the workspace it'll just regular workspace change.
 *
 * @method view_only
 * @tparam integer idx Index of the view
 * @noreturn
 * @see active_workspace
 */
static int luaC_screen_set_active_workspace(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    int new_view              = luaL_checknumber(L, 2);

    cwc_output_set_view_only(output, new_view);
    return 0;
}

/** Maximum general workspace (workspaces that will be shown in the bar).
 *
 * @property max_general_workspace
 * @tparam integer max_general_workspace
 * @propertydefault 9
 * @rangestart 1
 */
static int luaC_screen_get_max_general_workspace(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    lua_pushnumber(L, output->state->max_general_workspace);

    return 1;
}

static int luaC_screen_set_max_general_workspace(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    int newmax                = luaL_checkint(L, 2);
    newmax                    = MAX(MIN(newmax, MAX_WORKSPACE), 1);

    output->state->max_general_workspace = newmax;

    return 0;
}

/** Set useless gaps width in the active workspace.
 *
 * @property useless_gaps
 * @tparam integer useless_gaps
 * @propertydefault 0
 * @rangestart 0
 */
static int luaC_screen_get_useless_gaps(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    lua_pushnumber(L, cwc_output_get_current_view_info(output)->useless_gaps);

    return 1;
}

static int luaC_screen_set_useless_gaps(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    int width                 = luaL_checkint(L, 2);

    cwc_output_set_useless_gaps(output, 0, width);

    return 0;
}

/** Master width factor.
 *
 * @property mwfact
 * @tparam number mwfact
 * @propertydefault 0.5
 * @rangestart 0.1
 * @rangestop 0.9
 */
static int luaC_screen_get_mwfact(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    lua_pushnumber(
        L, cwc_output_get_current_view_info(output)->master_state.mwfact);

    return 1;
}

static int luaC_screen_set_mwfact(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    double factor             = luaL_checknumber(L, 2);

    cwc_output_set_mwfact(output, 0, factor);

    return 0;
}

/** Get containers in this screen.
 *
 * Ordered by time the container created (first item is newest to oldest).
 *
 * @method get_containers
 * @tparam[opt=false] bool visible Whether get only the visible containers
 * @treturn cwc_container[] array of container
 */
static int luaC_screen_get_containers(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    bool visible_only         = lua_toboolean(L, 2);

    lua_newtable(L);

    struct cwc_container *container;
    int i = 1;
    wl_list_for_each(container, &output->state->containers,
                     link_output_container)
    {
        if (visible_only) {
            if (cwc_container_is_visible(container))
                goto push_container;
        } else {
            goto push_container;
        }

        continue;

    push_container:
        luaC_object_push(L, container);
        lua_rawseti(L, -2, i++);
    }

    return 1;
}

/** Get toplevels/clients in this screen.
 *
 * Ordered by time the toplevel mapped (first item is newest to oldest).
 *
 * @method get_clients
 * @tparam[opt=false] bool visible Whether get only the visible toplevel
 * @treturn cwc_client[] array of toplevels
 */
static int luaC_screen_get_clients(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    bool visible_only         = lua_toboolean(L, 2);

    lua_newtable(L);

    struct cwc_toplevel *toplevel;
    int i = 1;
    wl_list_for_each(toplevel, &output->state->toplevels, link_output_toplevels)
    {
        if (visible_only) {
            if (cwc_toplevel_is_visible(toplevel))
                goto push_toplevel;
        } else {
            goto push_toplevel;
        }

        continue;

    push_toplevel:
        luaC_object_push(L, toplevel);
        lua_rawseti(L, -2, i++);
    }

    return 1;
}

/** get focus stack in the output
 *
 * @method get_focus_stack
 * @tparam[opt=false] bool visible Whether get only the visible toplevel
 * @treturn cwc_client[] array of toplevels
 */
static int luaC_screen_get_focus_stack(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    bool visible_only         = lua_toboolean(L, 2);

    lua_newtable(L);

    struct cwc_container *container;
    int i = 1;
    wl_list_for_each(container, &output->state->focus_stack, link_output_fstack)
    {
        if (visible_only) {
            if (cwc_container_is_visible(container))
                goto push_toplevel;
        } else {
            goto push_toplevel;
        }

        continue;

    push_toplevel:
        luaC_object_push(L, cwc_container_get_front_toplevel(container));
        lua_rawseti(L, -2, i++);
    }

    return 1;
}

/** get minimized stack in the output.
 *
 * The order is newest minimized to oldest.
 *
 * @method get_minimized
 * @tparam[opt=false] bool active Whether to use active_tag as filter
 * @treturn cwc_client[] array of toplevels
 */
static int luaC_screen_get_minimized(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    bool activetag_only       = lua_toboolean(L, 2);

    lua_newtable(L);

    struct cwc_container *container;
    int i = 1;
    wl_list_for_each(container, &output->state->minimized, link_minimized)
    {
        if (activetag_only) {
            if (container->tag & output->state->active_tag)
                goto push_toplevel;
        } else {
            goto push_toplevel;
        }

        continue;

    push_toplevel:
        luaC_object_push(L, cwc_container_get_front_toplevel(container));
        lua_rawseti(L, -2, i++);
    }

    return 1;
}

#define SCREEN_REG_READ_ONLY(name) {"get_" #name, luaC_screen_get_##name}
#define SCREEN_REG_SETTER(name)    {"set_" #name, luaC_screen_set_##name}
#define SCREEN_REG_PROPERTY(name) \
    SCREEN_REG_READ_ONLY(name), SCREEN_REG_SETTER(name)

void luaC_screen_setup(lua_State *L)
{
    luaL_Reg screen_metamethods[] = {
        {"__eq",       luaC_screen_eq      },
        {"__tostring", luaC_screen_tostring},
        {NULL,         NULL                },
    };

    luaL_Reg screen_methods[] = {
        {"view_only",       luaC_screen_set_active_workspace},
        {"toggle_tag",      luaC_screen_toggle_tag          },
        {"strategy_idx",    luaC_screen_strategy_idx        },

        // ro prop but have arguments
        {"get_containers",  luaC_screen_get_containers      },
        {"get_clients",     luaC_screen_get_clients         },
        {"get_focus_stack", luaC_screen_get_focus_stack     },
        {"get_minimized",   luaC_screen_get_minimized       },

        // readonly prop
        SCREEN_REG_READ_ONLY(name),
        SCREEN_REG_READ_ONLY(description),
        SCREEN_REG_READ_ONLY(make),
        SCREEN_REG_READ_ONLY(model),
        SCREEN_REG_READ_ONLY(serial),
        SCREEN_REG_READ_ONLY(enabled),
        SCREEN_REG_READ_ONLY(non_desktop),
        SCREEN_REG_READ_ONLY(workarea),
        SCREEN_REG_READ_ONLY(width),
        SCREEN_REG_READ_ONLY(height),
        SCREEN_REG_READ_ONLY(refresh),
        SCREEN_REG_READ_ONLY(phys_width),
        SCREEN_REG_READ_ONLY(phys_height),
        SCREEN_REG_READ_ONLY(scale),
        SCREEN_REG_READ_ONLY(restored),

        // rw properties
        SCREEN_REG_PROPERTY(allow_tearing),
        SCREEN_REG_PROPERTY(layout_mode),
        SCREEN_REG_PROPERTY(active_tag),
        SCREEN_REG_PROPERTY(active_workspace),
        SCREEN_REG_PROPERTY(max_general_workspace),
        SCREEN_REG_PROPERTY(useless_gaps),
        SCREEN_REG_PROPERTY(mwfact),

        {NULL,              NULL                            },
    };

    luaC_register_class(L, screen_classname, screen_methods,
                        screen_metamethods);

    luaL_Reg screen_staticlibs[] = {
        {"get",               luaC_screen_get                     },
        {"focused",           luaC_screen_focused                 },
        {"at",                luaC_screen_at                      },

        {"get_max_workspace", luaC_screen_get_max_workspace       },
        {"set_useless_gaps",  luaC_screen_set_default_useless_gaps},
        {NULL,                NULL                                },
    };

    lua_newtable(L);
    luaL_register(L, NULL, screen_staticlibs);
    lua_setfield(L, -2, "screen");
}
