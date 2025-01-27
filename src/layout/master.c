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
#include <wlr/types/wlr_cursor.h>

#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/input/cursor.h"
#include "cwc/layout/bsp.h"
#include "cwc/layout/container.h"
#include "cwc/layout/master.h"
#include "cwc/types.h"
#include "cwc/util.h"

enum stage { START, UPDATE, END };

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
    struct wlr_box usable_area = output->usable_area;

    /* master */
    int master_count = master_state->master_count;
    int master_width = master_count >= len
                           ? usable_area.width
                           : usable_area.width * master_state->mwfact;
    master_count     = master_count >= len ? len : master_count;

    int start_x = usable_area.x;
    int start_y = usable_area.y;

    float total_fact = 0;

    /* get sum of wfact */
    for (int i = 0; i < master_count; i++) {
        struct cwc_toplevel *elem = toplevels[i];
        total_fact += elem->container->wfact;
    }

    int next_y = start_y;
    for (int i = 0; i < (master_count - 1); i++) {
        struct cwc_toplevel *elem = toplevels[i];

        int height = usable_area.height * elem->container->wfact / total_fact;
        struct wlr_box newgeom = {start_x, next_y, master_width, height};

        cwc_container_set_box_gap(elem->container, &newgeom);
        next_y += height;
    }

    /* last element fill remaining area */
    {
        struct wlr_box newgeom = {start_x, next_y, master_width,
                                  usable_area.height - next_y + start_y};
        cwc_container_set_box_gap(toplevels[master_count - 1]->container,
                                  &newgeom);
    }

    if (master_count >= len)
        return;

    /* secondary */
    int sec_len   = len - master_count;
    int col_count = master_state->column_count >= sec_len
                        ? sec_len
                        : master_state->column_count;
    int sec_width = usable_area.width - master_width;

    int col_capacities[col_count];
    int min_item_per_col = sec_len / col_count;
    int item_remainder   = sec_len % col_count;

    for (int i = master_state->column_count - 1; i >= 0; i--) {
        col_capacities[i] = min_item_per_col;

        if (item_remainder >= 1) {
            col_capacities[i]++;
            item_remainder--;
        }
    }

    int next_x     = master_width;
    int col_width  = sec_width / col_count;
    int cidx_start = master_count;

    /* the logic is identical like master but applied for each column */
    for (int col = 0; col < col_count; col++) {
        int col_cap  = col_capacities[col];
        int cidx_end = cidx_start + col_cap;
        next_y       = start_y;
        total_fact   = 0;

        for (int i = cidx_start; i < cidx_end; i++) {
            struct cwc_toplevel *elem = toplevels[i];
            total_fact += elem->container->wfact;
        }

        for (int i = cidx_start; i < cidx_end - 1; i++) {
            struct cwc_toplevel *elem = toplevels[i];

            int height =
                usable_area.height * elem->container->wfact / total_fact;
            struct wlr_box newgeom = {next_x, next_y, col_width, height};

            cwc_container_set_box_gap(elem->container, &newgeom);
            next_y += height;
        }

        {
            struct wlr_box newgeom = {next_x, next_y, col_width,
                                      usable_area.height - next_y + start_y};
            cwc_container_set_box_gap(toplevels[cidx_end - 1]->container,
                                      &newgeom);
        }

        next_x += col_width;
        cidx_start += col_cap;
    }
}

struct resize_data_tile {
    double init_mwfact;
} resize_data_tile = {0};

static void resize_tile_start(struct cwc_toplevel **toplevels,
                              int len,
                              struct cwc_cursor *cursor,
                              struct master_state *master_state)
{
    struct cwc_output *output = cursor->grabbed_toplevel->container->output;
    wlr_cursor_warp(cursor->wlr_cursor, NULL,
                    output->usable_area.width * master_state->mwfact,
                    cursor->wlr_cursor->y);
    cursor->grab_x = cursor->wlr_cursor->x;
    cursor->grab_y = cursor->wlr_cursor->y;

    resize_data_tile.init_mwfact = master_state->mwfact;

    cwc_cursor_set_image_by_name(cursor, "col-resize");
}

static void resize_tile_update(struct cwc_toplevel **toplevels,
                               int len,
                               struct cwc_cursor *cursor,
                               struct master_state *master_state)
{
    struct cwc_toplevel *toplevel = cursor->grabbed_toplevel;
    struct cwc_output *output     = toplevel->container->output;
    double cx                     = cursor->wlr_cursor->x;
    double diff_x                 = cx - cursor->grab_x;

    master_state->mwfact =
        CLAMP(resize_data_tile.init_mwfact + diff_x / output->usable_area.width,
              0.1, 0.9);
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
    tile_impl->resize_start            = resize_tile_start;
    tile_impl->resize_update           = resize_tile_update;

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

static int get_tiled_toplevel_array(struct cwc_output *output,
                                    struct cwc_toplevel **toplevels,
                                    int array_len)
{
    int i = 0;
    struct cwc_container *container;
    wl_list_for_each(container, &output->state->containers,
                     link_output_container)
    {
        struct cwc_toplevel *front =
            cwc_container_get_front_toplevel(container);
        if (cwc_toplevel_is_tileable(front))
            toplevels[i++] = front;

        // sanity check
        if (i >= array_len - 1)
            break;
    }

    toplevels[i] = NULL;
    return i;
}

void master_arrange_update(struct cwc_output *output)
{
    struct cwc_tag_info *info = cwc_output_get_current_tag_info(output);
    if (info->layout_mode != CWC_LAYOUT_MASTER)
        return;

    struct master_state *state = &info->master_state;

    struct cwc_toplevel *tiled_visible[50];
    int i = get_tiled_toplevel_array(output, tiled_visible, 50);

    if (i >= 1)
        state->current_layout->arrange(tiled_visible, i, output, state);
}

static void _master_resize(struct cwc_output *output,
                           struct cwc_cursor *cursor,
                           enum stage stage)
{
    struct master_state *state =
        &cwc_output_get_current_tag_info(output)->master_state;
    struct layout_interface *layout = state->current_layout;

    struct cwc_toplevel *tiled_visible[50];
    int i = get_tiled_toplevel_array(output, tiled_visible, 50);

    if (layout->resize_update && stage == UPDATE)
        layout->resize_update(tiled_visible, i, cursor, state);
    else if (layout->resize_start && stage == START)
        layout->resize_start(tiled_visible, i, cursor, state);
    else if (layout->resize_end && stage == END)
        layout->resize_end(tiled_visible, i, cursor, state);

    master_arrange_update(output);
}

void master_resize_start(struct cwc_output *output, struct cwc_cursor *cursor)
{
    _master_resize(output, cursor, START);
}

void master_resize_update(struct cwc_output *output, struct cwc_cursor *cursor)
{
    _master_resize(output, cursor, UPDATE);
}

void master_resize_end(struct cwc_output *output, struct cwc_cursor *cursor)
{
    _master_resize(output, cursor, END);
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
