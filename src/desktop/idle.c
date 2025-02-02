/* idle.c - define idle behavior
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

#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_layer_shell_v1.h>

#include "cwc/desktop/idle.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/server.h"
#include "cwc/util.h"

/* return the inhibitor count that visible */
static int get_valid_idle_inhibitor_count()
{
    int count = 0;

    struct wlr_idle_inhibitor_v1 *inhibitor;
    wl_list_for_each(inhibitor, &server.idle->inhibit_manager->inhibitors, link)
    {
        struct cwc_toplevel *toplevel =
            cwc_toplevel_try_from_wlr_surface(inhibitor->surface);

        if (toplevel && !cwc_toplevel_is_visible(toplevel))
            continue;

        count++;
    }

    return count;
}

void update_idle_inhibitor(void *data)
{
    if (get_valid_idle_inhibitor_count() < 1)
        wlr_idle_notifier_v1_set_inhibited(server.idle->idle_notifier, false);
    else
        wlr_idle_notifier_v1_set_inhibited(server.idle->idle_notifier, true);
}

static void on_destroy_inhibitor(struct wl_listener *listener, void *data)
{
    LISTEN_DESTROY(listener);
    cwc_log(CWC_DEBUG, "idle inhibitor destroyed: %p", data);

    /* let wlroots remove the inhibitors in the list first before updating */
    wl_event_loop_add_idle(server.wl_event_loop, update_idle_inhibitor, data);
}

static void on_new_inhibitor(struct wl_listener *listener, void *data)
{
    struct cwc_idle *idle = wl_container_of(listener, idle, new_inhibitor_l);
    struct wlr_idle_inhibitor_v1 *wlr_inhibitor = data;
    wlr_inhibitor->data                         = idle;

    cwc_log(CWC_DEBUG, "idle inhibitor created: %p %p", wlr_inhibitor,
            wlr_inhibitor->surface);

    LISTEN_CREATE(&wlr_inhibitor->events.destroy, on_destroy_inhibitor);

    update_idle_inhibitor(NULL);
}

void cwc_idle_init(struct cwc_server *s)
{
    struct cwc_idle *idle = calloc(1, sizeof(*idle));
    s->idle               = idle;
    idle->server          = s;

    idle->inhibit_manager        = wlr_idle_inhibit_v1_create(s->wl_display);
    idle->idle_notifier          = wlr_idle_notifier_v1_create(s->wl_display);
    idle->new_inhibitor_l.notify = on_new_inhibitor;
    wl_signal_add(&idle->inhibit_manager->events.new_inhibitor,
                  &idle->new_inhibitor_l);
}

void cwc_idle_fini(struct cwc_server *s)
{
    wl_list_remove(&s->idle->new_inhibitor_l.link);
    free(s->idle);
    s->idle = NULL;
}
