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
 * See also:
 *
 * - `cuteful.tag`
 * - `cwc_tag`
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
#include "cwc/desktop/layer_shell.h"
#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/desktop/transaction.h"
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

/** Property signal.
 *
 * @signal screen::prop::active_tag
 * @tparam cwc_screen s The screen object.
 */

/** Property signal.
 *
 * @signal screen::prop::active_workspace
 * @tparam cwc_screen s The screen object.
 */

/** Property signal.
 *
 * @signal screen::prop::selected_tag
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

/** The screen coordinates in the global position.
 *
 * @property geometry
 * @tparam table geometry
 * @tparam integer geometry.x
 * @tparam integer geometry.y
 * @tparam integer geometry.width
 * @tparam integer geometry.height
 * @readonly
 */
static int luaC_screen_get_geometry(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);

    luaC_pushbox(L, output->output_layout_box);

    return 1;
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

/** The current selected tag of the screen.
 *
 * If there are 2 or more activated tags, the compositor will use this tag to
 * get information such as gaps, layout mode or master factor.
 *
 * @property selected_tag
 * @tparam cwc_tag selected_tag
 * @readonly
 * @propertydefault the last tag that set by view_only.
 * @see active_workspace
 * @see cwc_tag:view_only
 */
static int luaC_screen_get_selected_tag(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);

    luaC_object_push(L, cwc_output_get_current_tag_info(output));

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

/** Whether to allow screen to tear or not.
 *
 * @property allow_tearing
 * @tparam[opt=false] boolean allow_tearing
 * @see cwc.client.allow_tearing
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

    cwc_output_set_active_tag(output, newtag);

    return 0;
}

/** The index of the selected_tag.
 *
 * @property active_workspace
 * @tparam integer active_workspace
 * @propertydefault Current active workspace.
 * @negativeallowed false
 * @see selected_tag
 */
static int luaC_screen_get_active_workspace(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    lua_pushnumber(L, output->state->active_workspace);

    return 1;
}

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
 * @tparam[opt=false] bool visible Whether to use active_tag as filter
 * @treturn cwc_client[] array of toplevels
 */
static int luaC_screen_get_minimized(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    bool visible_only         = lua_toboolean(L, 2);

    lua_newtable(L);

    struct cwc_container *container;
    int i = 1;
    wl_list_for_each(container, &output->state->minimized,
                     link_output_minimized)
    {
        if (visible_only) {
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

/** Get tag with using the specified index.
 *
 * @method get_tag
 * @tparam integer idx Index of the tag.
 * @treturn[opt=nil] cwc_tag
 */
static int luaC_screen_get_tag(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    int tag                   = luaL_checkint(L, 2);

    if (tag >= 0 && tag <= MAX_WORKSPACE)
        luaC_object_push(L, &output->state->tag_info[tag]);
    else
        lua_pushnil(L);

    return 1;
}

/** Get nearest screen relative to this screen.
 *
 * @method get_nearest
 * @tparam integer direction Direction enum
 * @treturn cwc_screen
 * @see cuteful.enum.direction
 */
static int luaC_screen_get_nearest(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    int direction             = luaL_checkinteger(L, 2);

    luaC_object_push(L, cwc_output_get_nearest_by_direction(output, direction));

    return 1;
}

/** Set the screen position in the global coordinate.
 *
 * @method set_position
 * @tparam integer x Horizontal position
 * @tparam integer y Vertical position
 * @noreturn
 */
static int luaC_screen_set_position(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    int x                     = luaL_checkint(L, 2);
    int y                     = luaL_checkint(L, 3);

    cwc_output_set_position(output, x, y);

    return 0;
}

/** Set the screen mode.
 *
 * @method set_mode
 * @tparam integer width
 * @tparam integer height
 * @tparam[opt=0] integer refresh Monitor refresh rate in hz
 * @noreturn
 */
static int luaC_screen_set_mode(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    int32_t width             = luaL_checkint(L, 2);
    int32_t height            = luaL_checkint(L, 3);
    int32_t refresh           = lua_tonumber(L, 4);

    bool found = false;
    struct wlr_output_mode *mode;
    wl_list_for_each(mode, &output->wlr_output->modes, link)
    {
        if (mode->width == width && mode->height == height) {
            int diff = abs(refresh - mode->refresh / 1000);
            if (!refresh) {
                found = true;
                break;
            } else if (diff >= 0 && diff <= 2) {
                found = true;
                break;
            }
        }
    }

    if (!found)
        return 0;

    wlr_output_state_set_mode(&output->pending, mode);
    transaction_schedule_output(output);

    return 0;
}

/** Set adaptive sync.
 *
 * @method set_adaptive_sync
 * @tparam boolean enable True to enable
 * @noreturn
 */
static int luaC_screen_set_adaptive_sync(lua_State *L)
{
    luaL_checktype(L, 2, LUA_TBOOLEAN);
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    bool set                  = lua_toboolean(L, 2);

    if (!output->wlr_output->adaptive_sync_supported)
        return 0;

    wlr_output_state_set_adaptive_sync_enabled(&output->pending, set);
    transaction_schedule_output(output);

    return 0;
}

/** Set output enabled state.
 *
 * @method set_enabled
 * @tparam boolean enable True to enable
 * @noreturn
 */
static int luaC_screen_set_enabled(lua_State *L)
{
    luaL_checktype(L, 2, LUA_TBOOLEAN);
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    bool set                  = lua_toboolean(L, 2);

    wlr_output_state_set_enabled(&output->pending, set);
    transaction_schedule_output(output);

    return 0;
}

/** Set output scale.
 *
 * @method set_scale
 * @tparam number scale
 * @noreturn
 */
static int luaC_screen_set_scale(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    float scale               = luaL_checknumber(L, 2);

    wlr_output_state_set_scale(&output->pending, scale);
    transaction_schedule_output(output);

    return 0;
}

/** Set output transform rotation.
 *
 * @method set_transform
 * @tparam integer enum Output transform enum
 * @noreturn
 * @see cuteful.enum.output_transform
 */
static int luaC_screen_set_transform(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);
    int transform_enum        = luaL_checkint(L, 2);

    wlr_output_state_set_transform(&output->pending, transform_enum);
    transaction_schedule_output(output);

    return 0;
}

/** Focus this screen.
 *
 * @method focus
 * @noreturn
 */
static int luaC_screen_focus(lua_State *L)
{
    struct cwc_output *output = luaC_screen_checkudata(L, 1);

    cwc_output_focus(output);

    return 0;
}

#define REG_METHOD(name)    {#name, luaC_screen_##name}
#define REG_READ_ONLY(name) {"get_" #name, luaC_screen_get_##name}
#define REG_SETTER(name)    {"set_" #name, luaC_screen_set_##name}
#define REG_PROPERTY(name)  REG_READ_ONLY(name), REG_SETTER(name)

void luaC_screen_setup(lua_State *L)
{
    luaL_Reg screen_metamethods[] = {
        {"__eq",       luaC_screen_eq      },
        {"__tostring", luaC_screen_tostring},
        {NULL,         NULL                },
    };

    luaL_Reg screen_methods[] = {
        REG_METHOD(focus),
        REG_METHOD(get_tag),
        REG_METHOD(get_nearest),
        REG_METHOD(set_position),

        // screen state
        REG_METHOD(set_mode),
        REG_METHOD(set_adaptive_sync),
        REG_METHOD(set_enabled),
        REG_METHOD(set_scale),
        REG_METHOD(set_transform),

        // ro prop but have optional arguments
        REG_METHOD(get_containers),
        REG_METHOD(get_clients),
        REG_METHOD(get_focus_stack),
        REG_METHOD(get_minimized),

        // readonly prop
        REG_READ_ONLY(geometry),
        REG_READ_ONLY(name),
        REG_READ_ONLY(description),
        REG_READ_ONLY(make),
        REG_READ_ONLY(model),
        REG_READ_ONLY(serial),
        REG_READ_ONLY(enabled),
        REG_READ_ONLY(non_desktop),
        REG_READ_ONLY(workarea),
        REG_READ_ONLY(width),
        REG_READ_ONLY(height),
        REG_READ_ONLY(refresh),
        REG_READ_ONLY(phys_width),
        REG_READ_ONLY(phys_height),
        REG_READ_ONLY(scale),
        REG_READ_ONLY(restored),
        REG_READ_ONLY(selected_tag),

        // rw properties
        REG_PROPERTY(allow_tearing),
        REG_PROPERTY(active_tag),
        REG_PROPERTY(active_workspace),
        REG_PROPERTY(max_general_workspace),

        {NULL, NULL},
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
