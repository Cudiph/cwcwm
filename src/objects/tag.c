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
#include <wayland-util.h>

#include "cwc/desktop/output.h"
#include "cwc/luaclass.h"

/** Index of the tag in the screen.
 *
 * @property index
 * @tparam integer index
 * @readonly
 * @rangestart 0
 * @rangestop MAX_WORKSPACE
 * @propertydefault static
 * @see cwc.screen.get_max_workspace
 */
static int luaC_tag_get_index(lua_State *L)
{
    struct cwc_tag_info *tag = luaC_tag_checkudata(L, 1);
    lua_pushnumber(L, tag->index);

    return 1;
}

/** True if the tag is selected to be viewed.
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

    cwc_output_update_visible(output);
    cwc_output_tiling_layout_update(output, 0);

    return 0;
}

/** Useless gaps width in this tag.
 *
 * @property useless_gaps
 * @tparam integer useless_gaps
 * @propertydefault 0
 * @rangestart 0
 */
static int luaC_tag_get_useless_gaps(lua_State *L)
{
    struct cwc_tag_info *tag = luaC_tag_checkudata(L, 1);
    lua_pushnumber(L, tag->useless_gaps);

    return 1;
}

static int luaC_tag_set_useless_gaps(lua_State *L)
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

/** Toggle selected tag.
 *
 * @method toggle
 * @noreturn
 */
static int luaC_tag_toggle(lua_State *L)
{
    struct cwc_tag_info *tag  = luaC_tag_checkudata(L, 1);
    struct cwc_output *output = cwc_output_from_tag_info(tag);

    output->state->active_tag ^= 1 << (tag->index - 1);
    cwc_output_update_visible(output);
    cwc_output_tiling_layout_update(output, 0);

    return 0;
}

/** Set this tag as the only selected tag.
 *
 * @method view_only
 * @tparam integer idx Index of the view
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

#define TAG_REG_READ_ONLY(name) {"get_" #name, luaC_tag_get_##name}
#define TAG_REG_SETTER(name)    {"set_" #name, luaC_tag_set_##name}
#define TAG_REG_PROPERTY(name)  TAG_REG_READ_ONLY(name), TAG_REG_SETTER(name)

void luaC_tag_setup(lua_State *L)
{
    luaL_Reg tag_metamethods[] = {
        {"__eq",       luaC_tag_eq      },
        {"__tostring", luaC_tag_tostring},
        {NULL,         NULL             },
    };

    luaL_Reg tag_methods[] = {
        {"toggle",       luaC_tag_toggle      },
        {"view_only",    luaC_tag_view_only   },
        {"strategy_idx", luaC_tag_strategy_idx},

        // ro prop
        TAG_REG_READ_ONLY(index),

        // properties
        TAG_REG_PROPERTY(selected),
        TAG_REG_PROPERTY(useless_gaps),
        TAG_REG_PROPERTY(mwfact),
        TAG_REG_PROPERTY(layout_mode),

        {NULL,           NULL                 },
    };

    luaC_register_class(L, tag_classname, tag_methods, tag_metamethods);
}
