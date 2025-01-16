/* switch.c - switch input device
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

#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_switch.h>

#include "cwc/input/seat.h"
#include "cwc/input/switch.h"

static void on_toggle(struct wl_listener *listener, void *data)
{
    struct cwc_switch *swt = wl_container_of(listener, swt, toggle_l);

    // TODO: xxx
}

static void on_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_switch *swt = wl_container_of(listener, swt, destroy_l);

    cwc_switch_destroy(swt);
}

struct cwc_switch *cwc_switch_create(struct cwc_seat *seat,
                                     struct wlr_input_device *dev)
{
    struct cwc_switch *swt = calloc(1, sizeof(*swt));
    if (!swt)
        return NULL;

    swt->seat             = seat;
    swt->wlr_switch       = wlr_switch_from_input_device(dev);
    swt->wlr_switch->data = swt;

    swt->toggle_l.notify = on_toggle;
    wl_signal_add(&swt->wlr_switch->events.toggle, &swt->toggle_l);

    swt->destroy_l.notify = on_destroy;
    wl_signal_add(&swt->wlr_switch->base.events.destroy, &swt->destroy_l);

    wl_list_insert(&seat->switch_devs, &swt->link);

    return swt;
}

void cwc_switch_destroy(struct cwc_switch *switch_dev)
{
    wl_list_remove(&switch_dev->toggle_l.link);
    wl_list_remove(&switch_dev->destroy_l.link);

    wl_list_remove(&switch_dev->link);

    free(switch_dev);
}
