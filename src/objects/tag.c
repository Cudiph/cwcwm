/* tag.c - lua cwc_tag object
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

/** Tag object for managing virtual workspace in a screen.
 *
 * To get the tag from the screen use `cwc.screen:get_tag`.
 *
 * The screen has its own tags ranged from 1-30, to query the
 * maximum workspace the compositor can handle use
 * `cwc.screen.get_max_workspace`. Tag index 0 is used for wallpaper view and
 * will not show any client
 *
 * @author Dwi Asmoro Bangun
 * @copyright 2024
 * @license GPLv3
 * @coreclassmod cwc_tag
 */

#include <lauxlib.h>
#include <lua.h>
#include <stdlib.h>
#include <wayland-util.h>
#include <wlr/types/wlr_ext_workspace_v1.h>

#include "cwc/desktop/output.h"
#include "cwc/desktop/transaction.h"
#include "cwc/layout/master.h"
#include "cwc/luaclass.h"
#include "cwc/luaobject.h"
#include "cwc/util.h"

/** Property signal.
 *
 * @signal tag::prop::label
 * @tparam cwc_tag tag The tag object.
 */

/** Property signal.
 *
 * @signal tag::prop::hidden
 * @tparam cwc_tag tag The tag object.
 */

//============================ CODE =================================

/** Index of the tag in the screen.
 *
 * @property index
 * @tparam integer index
 * @readonly
 * @rangestart 0
 * @rangestop MAX_WORKSPACE
 * @propertydefault static
 * @see cwc.screen.max_workspace
 */
static int luaC_tag_get_index(lua_State *L)
{
    struct cwc_tag_info *tag = luaC_tag_checkudata(L, 1);
    lua_pushnumber(L, tag->index);

    return 1;
}

/** The screen where this tag belongs.
 *
 * @property screen
 * @tparam cwc_screen screen
 * @readonly
 * @see cwc.screen
 * @propertydefault static
 */
static int luaC_tag_get_screen(lua_State *L)
{
    struct cwc_tag_info *tag  = luaC_tag_checkudata(L, 1);
    struct cwc_output *output = cwc_output_from_tag_info(tag);
    luaC_object_push(L, output);

    return 1;
}

/** The label or name of the tag.
 *
 * @property label
 * @tparam[opt=""] string label
 */
static int luaC_tag_get_label(lua_State *L)
{
    struct cwc_tag_info *tag = luaC_tag_checkudata(L, 1);
    lua_pushstring(L, tag->label);
    return 1;
}
static int luaC_tag_set_label(lua_State *L)
{
    struct cwc_tag_info *tag = luaC_tag_checkudata(L, 1);
    const char *newlabel     = luaL_checkstring(L, 2);

    cwc_tag_info_set_label(tag, newlabel);

    return 0;
}

/** True if the tag is selected to be viewed (active tag).
 *
 * @property selected
 * @tparam[opt=false] boolean selected
 */
static int luaC_tag_get_selected(lua_State *L)
{
    struct cwc_tag_info *tag  = luaC_tag_checkudata(L, 1);
    struct cwc_output *output = cwc_output_from_tag_info(tag);

    bool is_selected = output->state->active_tag & 1 << (tag->index - 1);
    lua_pushboolean(L, is_selected);

    return 1;
}

static int luaC_tag_set_selected(lua_State *L)
{
    struct cwc_tag_info *tag = luaC_tag_checkudata(L, 1);
    bool activate            = lua_toboolean(L, 2);

    struct cwc_output *output = cwc_output_from_tag_info(tag);
    if (activate)
        output->state->active_tag |= 1 << (tag->index - 1);
    else
        output->state->active_tag &= ~(1 << (tag->index - 1));

    transaction_schedule_output(output);
    transaction_schedule_tag(cwc_output_get_current_tag_info(output));

    return 0;
}

/** Hide the tag from the visual indicator.
 *
 * @property hidden
 * @tparam boolean hidden
 * @propertydefault `false` for general workspace.
 */
static int luaC_tag_get_hidden(lua_State *L)
{
    struct cwc_tag_info *tag = luaC_tag_checkudata(L, 1);

    lua_pushboolean(L, tag->hidden);

    return 1;
}
static int luaC_tag_set_hidden(lua_State *L)
{
    struct cwc_tag_info *tag = luaC_tag_checkudata(L, 1);
    bool set                 = lua_toboolean(L, 2);

    tag->hidden = set;
    if (tag->ext_workspace)
        wlr_ext_workspace_handle_v1_set_hidden(tag->ext_workspace, set);

    return 0;
}

/** The gap (spacing, also called useless_gap) between clients.
 *
 * @property gap
 * @tparam integer gap
 * @propertydefault 0
 * @rangestart 0
 */
static int luaC_tag_get_gap(lua_State *L)
{
    struct cwc_tag_info *tag = luaC_tag_checkudata(L, 1);
    lua_pushnumber(L, tag->useless_gaps);

    return 1;
}

static int luaC_tag_set_gap(lua_State *L)
{
    struct cwc_tag_info *tag = luaC_tag_checkudata(L, 1);
    int width                = luaL_checkint(L, 2);

    struct cwc_output *output = cwc_output_from_tag_info(tag);
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
static int luaC_tag_get_mwfact(lua_State *L)
{
    struct cwc_tag_info *tag = luaC_tag_checkudata(L, 1);
    lua_pushnumber(L, tag->master_state.mwfact);

    return 1;
}

static int luaC_tag_set_mwfact(lua_State *L)
{
    struct cwc_tag_info *tag = luaC_tag_checkudata(L, 1);
    double factor            = luaL_checknumber(L, 2);

    struct cwc_output *output = cwc_output_from_tag_info(tag);
    cwc_output_set_mwfact(output, tag->index, factor);

    return 0;
}

/** The layout mode of the selected workspace.
 *
 * @property layout_mode
 * @tparam[opt=0] integer layout_mode Layout mode enum from cuteful.
 * @negativeallowed false
 * @see cuteful.enum.layout_mode
 */
static int luaC_tag_set_layout_mode(lua_State *L)
{
    struct cwc_tag_info *tag = luaC_tag_checkudata(L, 1);
    uint32_t layout_mode     = luaL_checkint(L, 2);

    struct cwc_output *output = cwc_output_from_tag_info(tag);
    cwc_output_set_layout_mode(output, tag->index, layout_mode);

    return 0;
}

static int luaC_tag_get_layout_mode(lua_State *L)
{
    struct cwc_tag_info *tag = luaC_tag_checkudata(L, 1);

    lua_pushinteger(L, tag->layout_mode);

    return 1;
}

/** Set the number of master windows.
 *
 * @property master_count
 * @tparam[opt=1] integer master_count
 * @negativeallowed false
 */
static int luaC_tag_set_master_count(lua_State *L)
{
    struct cwc_tag_info *tag = luaC_tag_checkudata(L, 1);
    uint32_t master_count    = luaL_checkint(L, 2);

    tag->master_state.master_count = MAX(1, master_count);

    transaction_schedule_tag(tag);

    return 0;
}

static int luaC_tag_get_master_count(lua_State *L)
{
    struct cwc_tag_info *tag = luaC_tag_checkudata(L, 1);

    lua_pushinteger(L, tag->master_state.master_count);

    return 1;
}

/** Set the number of columns.
 *
 * @property column_count
 * @tparam[opt=1] integer column_count
 * @negativeallowed false
 */
static int luaC_tag_set_column_count(lua_State *L)
{
    struct cwc_tag_info *tag = luaC_tag_checkudata(L, 1);
    uint32_t column_count    = luaL_checkint(L, 2);

    tag->master_state.column_count = MAX(1, column_count);

    transaction_schedule_tag(tag);

    return 0;
}

static int luaC_tag_get_column_count(lua_State *L)
{
    struct cwc_tag_info *tag = luaC_tag_checkudata(L, 1);

    lua_pushinteger(L, tag->master_state.column_count);

    return 1;
}

/** Toggle selected tag.
 *
 * @method toggle
 * @noreturn
 */
static int luaC_tag_toggle(lua_State *L)
{
    struct cwc_tag_info *tag  = luaC_tag_checkudata(L, 1);
    struct cwc_output *output = cwc_output_from_tag_info(tag);

    tag_bitfield_t newtag = output->state->active_tag;
    newtag ^= 1 << (tag->index - 1);
    cwc_output_set_active_tag(output, newtag);

    return 0;
}

/** Set this tag as the only selected tag.
 *
 * @method view_only
 * @noreturn
 */
static int luaC_tag_view_only(lua_State *L)
{
    struct cwc_tag_info *tag = luaC_tag_checkudata(L, 1);

    struct cwc_output *output = cwc_output_from_tag_info(tag);
    cwc_output_set_view_only(output, tag->index);

    return 0;
}

/** Change the strategy in layout mode.
 *
 * @method strategy_idx
 * @tparam integer idx
 * @noreturn
 */
static int luaC_tag_strategy_idx(lua_State *L)
{
    struct cwc_tag_info *tag = luaC_tag_checkudata(L, 1);
    int direction            = luaL_checkint(L, 2);

    struct cwc_output *output = cwc_output_from_tag_info(tag);
    cwc_output_set_strategy_idx(output, direction);

    return 0;
}

#define REG_READ_ONLY(name) {"get_" #name, luaC_tag_get_##name}
#define REG_SETTER(name)    {"set_" #name, luaC_tag_set_##name}
#define REG_PROPERTY(name)  REG_READ_ONLY(name), REG_SETTER(name)

void luaC_tag_setup(lua_State *L)
{
    luaL_Reg tag_metamethods[] = {
        {"__eq",       luaC_tag_eq      },
        {"__tostring", luaC_tag_tostring},
        {NULL,         NULL             },
    };

    luaL_Reg tag_methods[] = {
        {"toggle",           luaC_tag_toggle      },
        {"view_only",        luaC_tag_view_only   },
        {"strategy_idx",     luaC_tag_strategy_idx},

        {"get_useless_gaps", luaC_tag_get_gap     },
        {"set_useless_gaps", luaC_tag_set_gap     },

        // ro prop
        REG_READ_ONLY(data),
        REG_READ_ONLY(index),
        REG_READ_ONLY(screen),

        // properties
        REG_PROPERTY(label),
        REG_PROPERTY(selected),
        REG_PROPERTY(hidden),
        REG_PROPERTY(gap),
        REG_PROPERTY(mwfact),
        REG_PROPERTY(layout_mode),
        REG_PROPERTY(master_count),
        REG_PROPERTY(column_count),

        {NULL,               NULL                 },
    };

    luaC_register_class(L, tag_classname, tag_methods, tag_metamethods);
}
