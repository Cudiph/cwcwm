/* transaction.c - actually just a simple scheduler
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

#include <wayland-server-core.h>
#include <wayland-util.h>

#include "cwc/desktop/layer_shell.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/desktop/transaction.h"
#include "cwc/layout/container.h"
#include "cwc/server.h"
#include "cwc/util.h"
#include "private/container.h"

static struct transaction {
    struct wl_event_source *idle_source;

    struct wl_array tags; // struct cwc_tag_info*

    bool output_pending;
    bool paused;
    bool processing; // prevent scheduling loop
} T = {0};

static inline void _process_pending_outputs(struct cwc_output *output)
{
    if (!cwc_output_is_exist(output) || !output->pending_transaction)
        return;

    if (output->pending.committed) {
        wlr_output_commit_state(output->wlr_output, &output->pending);
        wlr_output_state_finish(&output->pending);
        wlr_output_state_init(&output->pending);
    }

    arrange_layers(output);
    cwc_output_update_visible(output);
    output->pending_transaction = false;
}

static inline void _process_pending_tag(struct cwc_tag_info *tag)
{
    struct cwc_output *output = cwc_output_from_tag_info(tag);
    if (!cwc_output_is_exist(output))
        return;

    cwc_output_tiling_layout_update(output, tag->index);
    tag->pending_transaction = false;
}

static void process_pending(void *data)
{
    T.processing = true;

    if (T.output_pending) {
        struct cwc_output *output;
        wl_list_for_each(output, &server.outputs, link)
        {
            _process_pending_outputs(output);
        }

        cwc_output_update_outputs_state();
        T.output_pending = false;
    }

    if (T.tags.size) {
        struct cwc_tag_info **tag;
        wl_array_for_each(tag, &T.tags)
        {
            _process_pending_tag(*tag);
        }
        wl_array_release(&T.tags);
        wl_array_init(&T.tags);
    }

    T.idle_source = NULL;
    T.processing  = false;
}

static void transaction_start()
{
    if (T.idle_source || T.paused)
        return;

    T.idle_source =
        wl_event_loop_add_idle(server.wl_event_loop, process_pending, NULL);
}

void transaction_pause()
{
    if (T.idle_source)
        wl_event_source_remove(T.idle_source);

    T.idle_source = NULL;
    T.paused      = true;
}

void transaction_resume()
{
    T.paused = false;
    transaction_start();
}

void transaction_schedule_output(struct cwc_output *output)
{
    if (T.processing)
        return;

    transaction_start();
    output->pending_transaction = true;
    T.output_pending            = true;
}

void transaction_schedule_tag(struct cwc_tag_info *tag)
{
    if (tag->pending_transaction || T.processing)
        return;

    struct cwc_tag_info **elem = wl_array_add(&T.tags, sizeof(&tag));
    *elem                      = tag;
    tag->pending_transaction   = true;

    transaction_start();
}

void setup_transaction(struct cwc_server *s)
{
    wl_array_init(&T.tags);
}

void transaction_commit(struct cwc_toplevel *toplevel)
{
    struct cwc_container *container = toplevel->container;

    cwc_log(CWC_DEBUG, "committing toplevel (%p): %d %d %d %d", toplevel,
            container->pending.geom.x, container->pending.geom.y,
            container->pending.geom.width, container->pending.geom.height);
    wlr_scene_node_set_position(&container->tree->node,
                                container->pending.geom.x,
                                container->pending.geom.y);
    int gaps = cwc_container_get_gaps(container);
    cwc_border_resize(&container->border,
                      container->pending.geom.width - gaps * 2,
                      container->pending.geom.height - gaps * 2);

    if (wlr_box_empty(&toplevel->pending.clip)) {
        wlr_scene_subsurface_tree_set_clip(&toplevel->surf_tree->node, NULL);
    } else {
        toplevel->pending.clip.x = toplevel->xdg_toplevel->base->geometry.x,
        toplevel->pending.clip.y = toplevel->xdg_toplevel->base->geometry.y,
        wlr_scene_subsurface_tree_set_clip(&toplevel->surf_tree->node,
                                           &toplevel->pending.clip);
    }

    if (container->initializing && cwc_toplevel_is_visible(toplevel)) {
        cwc_container_set_opacity(container, container->opacity_before);
        container->initializing = false;
    }

    if (container->saved_tree) {
        wlr_scene_node_destroy(&container->saved_tree->node);
        container->saved_tree = NULL;
        wlr_scene_node_set_enabled(&toplevel->surf_tree->node, true);
    }

    cwc_log(CWC_DEBUG, "with clip (%p): %d %d %d %d", toplevel,
            toplevel->pending.clip.x, toplevel->pending.clip.y,
            toplevel->pending.clip.width, toplevel->pending.clip.height);

    container->current      = container->pending;
    toplevel->current       = toplevel->pending;
    toplevel->pending       = (struct cwc_toplevel_state){0};
    container->pending      = (struct cwc_container_state){0};
    toplevel->resize_serial = 0;
    toplevel->last_resize   = get_current_time_msec();

    cwc_container_update_output(container);
    wl_event_source_timer_update(container->resize_timer, 0);
}

static bool is_toplevel_ready(struct cwc_toplevel *toplevel)
{
    uint64_t timediff = get_current_time_msec() - toplevel->last_resize;
    if (timediff > RESIZE_TIMEOUT
        || toplevel->resize_serial
               <= toplevel->xdg_toplevel->base->current.configure_serial)
        return true;

    return false;
}

void transaction_check_commit(struct cwc_toplevel *toplevel)
{
    if (!is_toplevel_ready(toplevel))
        return;

    if ((!cwc_container_is_visible(toplevel->container)
         || cwc_toplevel_is_floating(toplevel)
         || !cwc_toplevel_is_configure_allowed(toplevel))) {
        transaction_commit(toplevel);
        return;
    }

    int resize_count = 0;
    struct cwc_vec *list_resize =
        cwc_vec_create(sizeof(struct cwc_toplevel *), 4);

    struct cwc_container *c;
    wl_list_for_each(c, &toplevel->container->output->state->containers,
                     link_output_container)
    {
        struct cwc_toplevel *front = cwc_container_get_front_toplevel(c);
        if (!front->resize_serial)
            continue;

        if (is_toplevel_ready(front)) {
            cwc_vec_push(list_resize, front);
            continue;
        }

        resize_count++;
        break;
    }

    if (resize_count)
        goto cleanup;

    for (int i = 0; i < list_resize->count; ++i) {
        struct cwc_toplevel *t = cwc_vec_at(list_resize, i);
        transaction_commit(t);
    }

    cwc_output_state_clear_saved_container(toplevel->container->output->state);

cleanup:
    cwc_vec_destroy(list_resize);
}
