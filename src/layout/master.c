/* master.c - master/stack layout operation
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

#include <wayland-util.h>

#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/layout/bsp.h"
#include "cwc/layout/container.h"
#include "cwc/layout/master.h"
#include "cwc/types.h"
#include "cwc/util.h"

static struct layout_interface *layout_list = NULL;

static void insert_impl(struct layout_interface *list,
                        struct layout_interface *elm)
{
    elm->prev       = list;
    elm->next       = list->next;
    list->next      = elm;
    elm->next->prev = elm;
}

static void remove_impl(struct layout_interface *elm)
{
    elm->prev->next = elm->next;
    elm->next->prev = elm->prev;
}

/* monocle layout */
static void arrange_monocle(struct cwc_toplevel **toplevels,
                            int len,
                            struct cwc_output *output,
                            struct master_state *master_state)
{
    int i                         = 0;
    struct cwc_toplevel *toplevel = toplevels[i];
    while (toplevel) {
        cwc_container_set_position_gap(
            toplevel->container, output->usable_area.x, output->usable_area.y);
        cwc_container_set_size(toplevel->container, output->usable_area.width,
                               output->usable_area.height);

        toplevel = toplevels[++i];
    }
}

static void master_register_monocle()
{
    struct layout_interface *monocle_impl = calloc(1, sizeof(*monocle_impl));
    monocle_impl->name                    = "monocle";
    monocle_impl->arrange                 = arrange_monocle;

    master_register_layout(monocle_impl);
}

/* tile layout */
static void arrange_tile(struct cwc_toplevel **toplevels,
                         int len,
                         struct cwc_output *output,
                         struct master_state *master_state)
{
    // TODO: account master count and column count
    struct wlr_box usable_area = output->usable_area;
    if (len == 1) {
        cwc_container_set_size(toplevels[0]->container,
                               output->usable_area.width, usable_area.height);
        cwc_container_set_position_gap(toplevels[0]->container, usable_area.x,
                                       usable_area.y);
        return;
    }

    int master_width = usable_area.width * master_state->mwfact;
    int sec_width    = usable_area.width - master_width;

    cwc_container_set_size(toplevels[0]->container, master_width,
                           usable_area.height);
    cwc_container_set_position_gap(toplevels[0]->container, usable_area.x,
                                   usable_area.y);

    int sec_count  = len - master_state->master_count;
    int sec_height = usable_area.height / sec_count;

    int height_used = 0;
    for (int i = 1; i < (len - 1); ++i) {
        struct cwc_toplevel *toplevel = toplevels[i];
        cwc_container_set_size(toplevel->container, sec_width, sec_height);
        cwc_container_set_position_gap(toplevel->container, master_width,
                                       height_used + usable_area.y);
        height_used += sec_height;
    }

    cwc_container_set_size(toplevels[len - 1]->container, sec_width,
                           usable_area.height - height_used);
    cwc_container_set_position_gap(toplevels[len - 1]->container, master_width,
                                   height_used + usable_area.y);
}

/* Initiialize the master/stack layout list since it doesn't have sentinel
 * head, an element must have inserted.
 */
static inline void master_init_layout_if_not_yet()
{
    if (layout_list)
        return;

    struct layout_interface *tile_impl = calloc(1, sizeof(*tile_impl));
    tile_impl->name                    = "tile";
    tile_impl->next                    = tile_impl;
    tile_impl->prev                    = tile_impl;
    tile_impl->arrange                 = arrange_tile;

    layout_list = tile_impl;

    // additional layout
    master_register_monocle();
}

void master_register_layout(struct layout_interface *impl)
{
    master_init_layout_if_not_yet();
    insert_impl(layout_list->prev, impl);
}

void master_unregister_layout(struct layout_interface *impl)
{
    remove_impl(impl);
}

struct layout_interface *get_default_master_layout()
{
    master_init_layout_if_not_yet();

    return layout_list;
}

void master_arrange_update(struct cwc_output *output)
{
    struct cwc_tag_info *info = cwc_output_get_current_tag_info(output);
    if (info->layout_mode != CWC_LAYOUT_MASTER)
        return;

    struct master_state *state = &info->master_state;

    struct cwc_toplevel *tiled_visible[50] = {0};
    int i                                  = 0;
    struct cwc_container *container;
    wl_list_for_each(container, &output->state->containers,
                     link_output_container)
    {
        struct cwc_toplevel *front =
            cwc_container_get_front_toplevel(container);
        if (cwc_toplevel_is_tileable(front))
            tiled_visible[i++] = front;

        // sanity check
        if (i > 49)
            break;
    }

    if (i >= 1)
        state->current_layout->arrange(tiled_visible, i, output, state);
}

struct cwc_toplevel *master_get_master(struct cwc_output *output)
{
    struct cwc_toplevel *toplevel;
    wl_list_for_each(toplevel, &output->state->toplevels, link_output_toplevels)
    {
        if (cwc_toplevel_is_tileable(toplevel))
            return toplevel;
    }

    return NULL;
}

void master_set_master(struct cwc_toplevel *toplevel)
{
    struct cwc_toplevel *master =
        master_get_master(toplevel->container->output);
    if (master == toplevel)
        return;

    wl_list_swap(&toplevel->link_output_toplevels,
                 &master->link_output_toplevels);

    master_arrange_update(toplevel->container->output);
}
