/* session_lock.c - session lock protocol implementation
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
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_session_lock_v1.h>

#include "cwc/desktop/output.h"
#include "cwc/desktop/session_lock.h"
#include "cwc/input/keyboard.h"
#include "cwc/server.h"

static void on_unlock(struct wl_listener *listener, void *data)
{
    struct cwc_session_locker *locker =
        wl_container_of(listener, locker, unlock_l);
    struct cwc_session_lock_manager *mgr = locker->manager;

    // unset state
    mgr->locked = false;
    cwc_output_focus_newest_focus_visible_toplevel(
        locker->lock_surface->output->data);
}

static void on_new_surface(struct wl_listener *listener, void *data)
{
    struct cwc_session_locker *locker =
        wl_container_of(listener, locker, new_surface_l);
    struct cwc_session_lock_manager *mgr             = locker->manager;
    struct wlr_session_lock_surface_v1 *lock_surface = data;

    struct cwc_output *output = lock_surface->output->data;

    wlr_scene_subsurface_tree_create(output->layers.session_lock,
                                     lock_surface->surface);

    wlr_session_lock_surface_v1_configure(
        lock_surface, output->wlr_output->width, output->wlr_output->height);

    // set state
    if (!mgr->locked) {
        mgr->locked          = true;
        mgr->locker          = locker;
        locker->lock_surface = lock_surface;
        wlr_session_lock_v1_send_locked(locker->locker);
        keyboard_focus_surface(server.seat, lock_surface->surface);
    }
}

static void on_lock_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_session_locker *locker =
        wl_container_of(listener, locker, destroy_l);

    wl_list_remove(&locker->new_surface_l.link);
    wl_list_remove(&locker->unlock_l.link);
    wl_list_remove(&locker->destroy_l.link);
    free(locker);
}

static void on_new_lock(struct wl_listener *listener, void *data)
{
    struct cwc_session_lock_manager *mgr =
        wl_container_of(listener, mgr, new_lock_l);
    struct wlr_session_lock_v1 *wlr_sesslock = data;

    struct cwc_session_locker *locker = calloc(1, sizeof(*locker));
    locker->locker                    = wlr_sesslock;
    locker->manager                   = mgr;

    locker->unlock_l.notify      = on_unlock;
    locker->new_surface_l.notify = on_new_surface;
    locker->destroy_l.notify     = on_lock_destroy;
    wl_signal_add(&wlr_sesslock->events.unlock, &locker->unlock_l);
    wl_signal_add(&wlr_sesslock->events.new_surface, &locker->new_surface_l);
    wl_signal_add(&wlr_sesslock->events.destroy, &locker->destroy_l);
}

static void on_session_lock_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_session_lock_manager *mgr =
        wl_container_of(listener, mgr, destroy_l);

    wl_list_remove(&mgr->new_lock_l.link);
    wl_list_remove(&mgr->destroy_l.link);
    mgr->manager              = NULL;
    mgr->server->session_lock = NULL;

    free(mgr);
}

void setup_cwc_session_lock(struct cwc_server *s)
{
    struct cwc_session_lock_manager *mgr = calloc(1, sizeof(*mgr));
    s->session_lock                      = mgr;
    mgr->manager = wlr_session_lock_manager_v1_create(s->wl_display);
    mgr->server  = s;

    mgr->new_lock_l.notify = on_new_lock;
    mgr->destroy_l.notify  = on_session_lock_destroy;
    wl_signal_add(&mgr->manager->events.new_lock, &mgr->new_lock_l);
    wl_signal_add(&mgr->manager->events.destroy, &mgr->destroy_l);
}
