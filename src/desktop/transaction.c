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

#include <stdio.h>
#include <wayland-server-core.h>
#include <wayland-util.h>

#include "cwc/desktop/layer_shell.h"
#include "cwc/desktop/transaction.h"
#include "cwc/server.h"

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
