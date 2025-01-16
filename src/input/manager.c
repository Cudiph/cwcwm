/* manager.c - input device management
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
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_switch.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/types/wlr_tablet_v2.h>
#include <wlr/types/wlr_touch.h>

#include "cwc/config.h"
#include "cwc/desktop/output.h"
#include "cwc/input/cursor.h"
#include "cwc/input/keyboard.h"
#include "cwc/input/manager.h"
#include "cwc/input/seat.h"
#include "cwc/luaclass.h"
#include "cwc/luaobject.h"
#include "cwc/server.h"
#include "cwc/signal.h"

// input manager singleton
static struct cwc_input_manager *input_manager = NULL;

static void on_libinput_device_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_libinput_device *dev = wl_container_of(listener, dev, destroy_l);

    lua_State *L = g_config_get_lua_State();
    cwc_object_emit_signal_simple("input::destroy", L, dev);
    luaC_object_unregister(L, dev);

    wl_list_remove(&dev->link);
    wl_list_remove(&dev->destroy_l.link);

    free(dev);
}

static void on_new_input(struct wl_listener *listener, void *data)
{
    struct cwc_input_manager *mgr = wl_container_of(listener, mgr, new_input_l);
    struct wlr_input_device *device = data;

    switch (device->type) {
    case WLR_INPUT_DEVICE_POINTER:
        cwc_seat_add_pointer_device(server.seat, device);
        break;
    case WLR_INPUT_DEVICE_KEYBOARD:
        cwc_seat_add_keyboard_device(server.seat, device);
        break;
    case WLR_INPUT_DEVICE_SWITCH:
        // cwc_seat_add_switch_device(server.seat, device);
        break;
    case WLR_INPUT_DEVICE_TABLET:
        // cwc_seat_add_tablet_device(server.seat, device);
        break;
    case WLR_INPUT_DEVICE_TABLET_PAD:
        // cwc_seat_add_tablet_pad_device(server.seat, device);
        break;
    case WLR_INPUT_DEVICE_TOUCH:
        // cwc_seat_add_touch_device(server.seat, device);
        break;
    }

    if (wlr_input_device_is_libinput(device)) {
        struct cwc_libinput_device *libinput_dev =
            calloc(1, sizeof(*libinput_dev));
        libinput_dev->device        = wlr_libinput_get_device_handle(device);
        libinput_dev->wlr_input_dev = device;

        libinput_dev->destroy_l.notify = on_libinput_device_destroy;
        wl_signal_add(&device->events.destroy, &libinput_dev->destroy_l);

        wl_list_insert(server.input->devices.prev, &libinput_dev->link);

        lua_State *L = g_config_get_lua_State();
        luaC_object_input_register(L, libinput_dev);
        cwc_object_emit_signal_simple("input::new", L, libinput_dev);
    }
}

struct cwc_input_manager *cwc_input_manager_get()
{
    if (input_manager)
        return input_manager;

    input_manager = calloc(1, sizeof(*input_manager));
    if (!input_manager)
        exit(EXIT_FAILURE);

    // input_manager->tablet_manager = wlr_tablet_v2_create(server.wl_display);

    wl_list_init(&input_manager->devices);
    wl_list_init(&input_manager->seats);

    input_manager->new_input_l.notify = on_new_input;
    wl_signal_add(&server.backend->events.new_input,
                  &input_manager->new_input_l);

    input_manager->relative_pointer_manager =
        wlr_relative_pointer_manager_v1_create(server.wl_display);

    return input_manager;
}

void cwc_input_manager_destroy()
{
    wl_list_remove(&input_manager->new_input_l.link);

    free(input_manager);
    input_manager = NULL;
}
