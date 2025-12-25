/* keyboard.c - keyboard processing
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

#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_keyboard_shortcuts_inhibit_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <xkbcommon/xkbcommon.h>

#include "cwc/config.h"
#include "cwc/desktop/idle.h"
#include "cwc/desktop/layer_shell.h"
#include "cwc/desktop/output.h"
#include "cwc/desktop/session_lock.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/input/cursor.h"
#include "cwc/input/keyboard.h"
#include "cwc/input/manager.h"
#include "cwc/input/seat.h"
#include "cwc/input/text_input.h"
#include "cwc/layout/bsp.h"
#include "cwc/luaclass.h"
#include "cwc/luaobject.h"
#include "cwc/server.h"
#include "cwc/signal.h"
#include "cwc/util.h"
#include "lua.h"

/**
 * Returns NULL if the keyboard is not grabbed by an input method,
 * or if event is from virtual keyboard of the same client as grab.
 */
static struct wlr_input_method_keyboard_grab_v2 *
keyboard_get_im_grab(struct cwc_seat *seat,
                     struct cwc_keyboard_group *kbd_group)
{
    struct cwc_input_method *im                      = seat->input_method;
    struct wlr_virtual_keyboard_v1 *virtual_keyboard = kbd_group->vkbd;

    if (!im || !im->wlr->keyboard_grab
        || (virtual_keyboard
            && wl_resource_get_client(virtual_keyboard->resource)
                   == wl_resource_get_client(
                       im->wlr->keyboard_grab->resource))) {
        return NULL;
    }
    return im->wlr->keyboard_grab;
}

static void process_modifier_event(struct cwc_keyboard_group *kbd_group)
{
    struct cwc_seat *seat        = kbd_group->seat;
    struct wlr_seat *wlr_seat    = seat->wlr_seat;
    struct wlr_keyboard *wlr_kbd = &kbd_group->wlr_kbd_group->keyboard;
    struct wlr_input_method_keyboard_grab_v2 *kbd_grab =
        keyboard_get_im_grab(seat, kbd_group);
    if (kbd_grab) {
        wlr_input_method_keyboard_grab_v2_set_keyboard(kbd_grab, wlr_kbd);
        wlr_input_method_keyboard_grab_v2_send_modifiers(kbd_grab,
                                                         &wlr_kbd->modifiers);
        return;
    }

    if (!kbd_group->send_events)
        return;

    wlr_seat_set_keyboard(wlr_seat, wlr_kbd);
    wlr_seat_keyboard_notify_modifiers(wlr_seat, &wlr_kbd->modifiers);
}

static void on_kbd_group_modifiers(struct wl_listener *listener, void *data)
{
    struct cwc_keyboard_group *kbd_group =
        wl_container_of(listener, kbd_group, modifiers_l);

    xkb_layout_index_t active_index =
        xkb_state_serialize_layout(kbd_group->wlr_kbd_group->keyboard.xkb_state,
                                   XKB_STATE_LAYOUT_EFFECTIVE);

    if (kbd_group->layout_idx != active_index) {
        cwc_object_emit_signal_simple("kbd::prop::layout_index",
                                      g_config_get_lua_State(), kbd_group);
        kbd_group->layout_idx = active_index;
    }

    process_modifier_event(kbd_group);
}

static void _send_kbd_key_signal(struct cwc_keyboard_group *kbd_group,
                                 struct wlr_keyboard_key_event *event,
                                 xkb_keysym_t keysym)
{
    lua_State *L = g_config_get_lua_State();
    if (!luaC_object_valid(L, kbd_group))
        return;

    char keyname[65] = {0};
    xkb_keysym_get_name(keysym, keyname, 64);

    uint32_t xkb_keycode = event->keycode + 8;
    lua_settop(L, 0);
    luaC_object_push(L, kbd_group);
    lua_pushnumber(L, event->time_msec);
    lua_pushnumber(L, xkb_keycode);
    lua_pushstring(L, keyname);

    struct cwc_keyboard_key_event cwc_event = {.kbd_group = kbd_group,
                                               .time_msec = event->time_msec,
                                               .keycode   = xkb_keycode};

    switch (event->state) {
    case WL_KEYBOARD_KEY_STATE_PRESSED:
        cwc_signal_emit("kbd::pressed", &cwc_event, L, 3);
        break;
    case WL_KEYBOARD_KEY_STATE_RELEASED:
        cwc_signal_emit("kbd::released", &cwc_event, L, 3);
        break;
    default:
        cwc_log(CWC_ERROR, "TODO: handle repeat");
        break;
    }
}

static void process_key_event(struct cwc_keyboard_group *kbd_group,
                              struct wlr_keyboard_key_event *event)
{
    struct cwc_seat *seat        = kbd_group->seat;
    struct wlr_keyboard *wlr_kbd = &kbd_group->wlr_kbd_group->keyboard;
    struct wlr_seat *wlr_seat    = seat->wlr_seat;

    wlr_idle_notifier_v1_notify_activity(server.idle->idle_notifier, wlr_seat);

    // translate libinput keycode -> xkbcommon
    int keycode = event->keycode + 8;

    // create an empty state to get untransformed keysym so that for keybinds we
    // don't need to account keyname change when the modifier is changed.
    // It will also look more readable e.g. MOD + "1" | MOD + SHIFT + "1"
    // while using transformed is MOD + "1" + MOD | SHIFT + "exclam".
    struct xkb_state *state = xkb_state_new(wlr_kbd->keymap);
    int keysym              = xkb_state_key_get_one_sym(state, keycode);
    xkb_state_unref(state);

    uint32_t modifiers = wlr_keyboard_get_modifiers(wlr_kbd);
    bool handled       = 0;

    struct cwc_keybind_map *kmap;
    switch (event->state) {
    case WL_KEYBOARD_KEY_STATE_PRESSED:
        handled |= keybind_kbd_execute(server.main_kbd_kmap, seat, modifiers,
                                       keysym, true);
        wl_list_for_each(kmap, &server.kbd_kmaps, link)
        {
            if (kmap->active)
                handled |=
                    keybind_kbd_execute(kmap, seat, modifiers, keysym, true);
        }
        break;
    case WL_KEYBOARD_KEY_STATE_RELEASED:
        // always notify released to client even when has keybind because there
        // is a case when you hold down a key and hitting keybind while still
        // holding it, the client still assume that key is pressed because
        // client doesn't get notified when the key is released
        // TODO: stupid hack #1
        keybind_kbd_execute(server.main_kbd_kmap, seat, modifiers, keysym,
                            false);
        wl_event_source_timer_update(server.main_kbd_kmap->repeat_timer, 0);
        server.main_kbd_kmap->repeated_bind = NULL;

        wl_list_for_each(kmap, &server.kbd_kmaps, link)
        {
            wl_event_source_timer_update(kmap->repeat_timer, 0);
            kmap->repeated_bind = NULL;

            if (kmap->active)
                keybind_kbd_execute(kmap, seat, modifiers, keysym, false);
        }
        break;
    default:
        cwc_log(CWC_ERROR, "TODO: HANDLE REPEAT");
        break;
    }

    if (!kbd_group->send_events)
        goto send_signal;

    if (!handled) {
        struct wlr_input_method_keyboard_grab_v2 *kbd_grab =
            keyboard_get_im_grab(seat, kbd_group);

        if (kbd_grab) {
            wlr_input_method_keyboard_grab_v2_set_keyboard(kbd_grab, wlr_kbd);
            wlr_input_method_keyboard_grab_v2_send_key(
                kbd_grab, event->time_msec, event->keycode, event->state);
            handled = true;

            // TODO: stupid hack #2
            if (event->state == WL_KEYBOARD_KEY_STATE_RELEASED)
                wlr_seat_keyboard_notify_key(wlr_seat, event->time_msec,
                                             event->keycode, event->state);
        }
    }

    if (!handled) {
        wlr_seat_set_keyboard(wlr_seat, wlr_kbd);
        wlr_seat_keyboard_notify_key(wlr_seat, event->time_msec, event->keycode,
                                     event->state);
    }

send_signal:
    if (kbd_group->grab)
        _send_kbd_key_signal(kbd_group, event, keysym);
}

static void on_kbd_group_key(struct wl_listener *listener, void *data)
{
    struct cwc_keyboard_group *kbd_group =
        wl_container_of(listener, kbd_group, key_l);
    struct wlr_keyboard_key_event *event = data;

    process_key_event(kbd_group, event);
}

static void _notify_focus_signal(struct wlr_surface *old_surface,
                                 struct wlr_surface *new_surface)
{
    struct cwc_toplevel *old = cwc_toplevel_try_from_wlr_surface(old_surface);
    struct cwc_toplevel *new = cwc_toplevel_try_from_wlr_surface(new_surface);

    if (new) {
        if (new->container->bsp_node)
            bsp_last_focused_update(new->container);

        if (new->wlr_foreign_handle)
            wlr_foreign_toplevel_handle_v1_set_activated(
                new->wlr_foreign_handle, true);

        if (cwc_toplevel_is_unmanaged(new))
            return;
    }

    if (old) {
        if (old->wlr_foreign_handle)
            wlr_foreign_toplevel_handle_v1_set_activated(
                old->wlr_foreign_handle, false);
    }

    // only emit signal when mapped
    if (old && cwc_toplevel_is_mapped(old)) {
        if (cwc_toplevel_is_unmanaged(old))
            return;

        cwc_toplevel_set_activated(old, false);
        cwc_object_emit_signal_simple("client::unfocus",
                                      g_config_get_lua_State(), old);
    }

    if (new && cwc_toplevel_is_mapped(new)) {
        cwc_object_emit_signal_simple("client::focus", g_config_get_lua_State(),
                                      new);
    }
}

void on_keyboard_focus_change(struct wl_listener *listener, void *data)
{
    struct cwc_seat *seat =
        wl_container_of(listener, seat, keyboard_focus_change_l);
    struct wlr_seat_keyboard_focus_change_event *event = data;
    struct cwc_output *focused_output                  = server.focused_output;

    // check for exclusive focus
    if (server.session_lock->locked && focused_output->lock_surface) {
        keyboard_focus_surface(seat, focused_output->lock_surface->surface);
        return;
    } else if (seat->exclusive_kbd_interactive) {
        keyboard_focus_surface(
            seat, seat->exclusive_kbd_interactive->wlr_layer_surface->surface);
        return;
    }

    _notify_focus_signal(event->old_surface, event->new_surface);

    if (seat->input_method)
        text_input_try_focus_surface(seat, event->new_surface);
}

static void apply_config(struct wlr_keyboard *kbd)
{
    wlr_keyboard_set_repeat_info(kbd, g_config.repeat_rate,
                                 g_config.repeat_delay);
}

static void on_config_commit(struct wl_listener *listener, void *data)
{
    struct cwc_keyboard_group *kbd_group =
        wl_container_of(listener, kbd_group, config_commit_l);

    apply_config(&kbd_group->wlr_kbd_group->keyboard);
}

void cwc_keyboard_update_keymap(struct wlr_keyboard *wlr_kbd)
{
    struct xkb_context *ctx     = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_rule_names names = {
        .rules   = g_config.xkb_rules,
        .model   = g_config.xkb_model,
        .layout  = g_config.xkb_layout,
        .variant = g_config.xkb_variant,
        .options = g_config.xkb_options,
    };
    struct xkb_keymap *keymap =
        xkb_keymap_new_from_names(ctx, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);

    wlr_keyboard_set_keymap(wlr_kbd, keymap);

    xkb_keymap_unref(keymap);
    xkb_context_unref(ctx);
}

struct cwc_keyboard_group *
cwc_keyboard_group_create(struct cwc_seat *seat,
                          struct wlr_virtual_keyboard_v1 *virtual)
{
    struct cwc_keyboard_group *kbd_group = calloc(1, sizeof(*kbd_group));
    if (!kbd_group)
        return NULL;

    cwc_log(CWC_DEBUG, "creating keyboard group: %p", kbd_group);

    kbd_group->wlr_kbd_group                = wlr_keyboard_group_create();
    kbd_group->wlr_kbd_group->keyboard.data = kbd_group;
    kbd_group->seat                         = seat;
    kbd_group->send_events                  = true;

    kbd_group->modifiers_l.notify = on_kbd_group_modifiers;
    kbd_group->key_l.notify       = on_kbd_group_key;
    wl_signal_add(&kbd_group->wlr_kbd_group->keyboard.events.modifiers,
                  &kbd_group->modifiers_l);
    wl_signal_add(&kbd_group->wlr_kbd_group->keyboard.events.key,
                  &kbd_group->key_l);

    if (virtual) {
        kbd_group->vkbd = virtual;
        wl_list_init(&kbd_group->config_commit_l.link);
    } else {
        kbd_group->config_commit_l.notify = on_config_commit;
        wl_signal_add(&g_config.events.commit, &kbd_group->config_commit_l);
    }
    wl_list_init(&kbd_group->keyboards);

    struct wlr_keyboard *wlr_kbd = &kbd_group->wlr_kbd_group->keyboard;
    wlr_seat_set_keyboard(seat->wlr_seat, wlr_kbd);
    cwc_keyboard_update_keymap(wlr_kbd);
    apply_config(wlr_kbd);

    lua_State *L = g_config_get_lua_State();
    luaC_object_kbd_register(L, kbd_group);

    return kbd_group;
}

void cwc_keyboard_group_destroy(struct cwc_keyboard_group *kbd_group)
{
    cwc_log(CWC_DEBUG, "destroying keyboard group: %p", kbd_group);

    lua_State *L = g_config_get_lua_State();
    luaC_object_unregister(L, kbd_group);

    wl_list_remove(&kbd_group->modifiers_l.link);
    wl_list_remove(&kbd_group->key_l.link);

    wl_list_remove(&kbd_group->config_commit_l.link);

    wlr_keyboard_group_destroy(kbd_group->wlr_kbd_group);
    free(kbd_group);
}

void cwc_keyboard_group_add_device(struct cwc_keyboard_group *kbd_group,
                                   struct wlr_input_device *device)
{
    struct wlr_keyboard *wlr_kbd = wlr_keyboard_from_input_device(device);

    struct cwc_keyboard *kbd = calloc(1, sizeof(*kbd));
    kbd->wlr_kbd             = wlr_kbd;
    wlr_kbd->data            = kbd;
    wl_list_insert(&kbd_group->keyboards, &kbd->link);

    cwc_keyboard_update_keymap(wlr_kbd);

    wlr_keyboard_group_add_keyboard(kbd_group->wlr_kbd_group, wlr_kbd);
}

void cwc_keyboard_group_remove_device(struct cwc_keyboard_group *kbd_group,
                                      struct wlr_input_device *device)
{
    struct wlr_keyboard *wlr_kbd = wlr_keyboard_from_input_device(device);
    struct cwc_keyboard *kbd     = wlr_kbd->data;
    kbd->wlr_kbd                 = wlr_kbd;

    wl_list_remove(&kbd->link);
    free(kbd);
    wlr_keyboard_group_remove_keyboard(kbd_group->wlr_kbd_group, wlr_kbd);
}

static inline void _update_modifiers(struct cwc_keyboard_group *kbd_group,
                                     uint32_t depressed,
                                     uint32_t latched,
                                     uint32_t locked,
                                     uint32_t group)
{
    struct cwc_keyboard *kbd;
    wl_list_for_each(kbd, &kbd_group->keyboards, link)
    {
        wlr_keyboard_notify_modifiers(kbd->wlr_kbd, depressed, latched, locked,
                                      group);
    }

    wlr_keyboard_notify_modifiers(&kbd_group->wlr_kbd_group->keyboard,
                                  depressed, latched, locked, group);
}

void cwc_keyboard_group_set_xkb_layout(struct cwc_keyboard_group *kbd_group,
                                       int idx)
{
    struct wlr_keyboard *wlr_kbd = &kbd_group->wlr_kbd_group->keyboard;
    struct xkb_state *state      = wlr_kbd->xkb_state;
    int num_layout               = xkb_keymap_num_layouts(wlr_kbd->keymap);
    int new_group                = idx % num_layout;

    uint32_t depressed_mods =
        xkb_state_serialize_mods(state, XKB_STATE_MODS_DEPRESSED);
    uint32_t latched_mods =
        xkb_state_serialize_mods(state, XKB_STATE_MODS_LATCHED);
    uint32_t locked_mods =
        xkb_state_serialize_mods(state, XKB_STATE_MODS_LOCKED);

    _update_modifiers(kbd_group, depressed_mods, latched_mods, locked_mods,
                      new_group);
}

void cwc_keyboard_group_update_modifiers(struct cwc_keyboard_group *kbd_group,
                                         uint32_t depressed,
                                         uint32_t latched,
                                         uint32_t locked)
{
    struct xkb_state *state = kbd_group->wlr_kbd_group->keyboard.xkb_state;

    uint32_t group =
        xkb_state_serialize_layout(state, XKB_STATE_LAYOUT_EFFECTIVE);

    _update_modifiers(kbd_group, depressed, latched, locked, group);
}

static void __cwc_keyboard_group_send_key(struct cwc_keyboard_group *kbd_group,
                                          uint32_t keycode,
                                          enum wl_keyboard_key_state state,
                                          bool raw)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t now_msec = timespec_to_msec(&now);

    struct wlr_keyboard_key_event event = {
        .keycode      = keycode,
        .state        = state,
        .time_msec    = now_msec,
        .update_state = true,
    };

    wlr_keyboard_notify_key(&kbd_group->wlr_kbd_group->keyboard, &event);
    if (raw) {
        wlr_seat_keyboard_notify_key(kbd_group->seat->wlr_seat, now_msec,
                                     keycode, state);
    } else {
        process_key_event(kbd_group, &event);
    }
}

void cwc_keyboard_group_send_key(struct cwc_keyboard_group *kbd_group,
                                 uint32_t keycode,
                                 enum wl_keyboard_key_state state)
{
    __cwc_keyboard_group_send_key(kbd_group, keycode, state, false);
}

void cwc_keyboard_group_send_key_raw(struct cwc_keyboard_group *kbd_group,
                                     uint32_t keycode,
                                     enum wl_keyboard_key_state state)
{
    __cwc_keyboard_group_send_key(kbd_group, keycode, state, true);
}

inline void keyboard_focus_surface(struct cwc_seat *seat,
                                   struct wlr_surface *surface)
{
    struct wlr_keyboard *kbd = wlr_seat_get_keyboard(seat->wlr_seat);
    if (kbd && surface)
        wlr_seat_keyboard_notify_enter(seat->wlr_seat, surface, kbd->keycodes,
                                       kbd->num_keycodes, &kbd->modifiers);
    else
        wlr_seat_keyboard_notify_enter(seat->wlr_seat, surface, NULL, 0, NULL);
}

static void on_vkbd_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_virtual_keyboard *kbd =
        wl_container_of(listener, kbd, destroy_l);

    cwc_log(WLR_DEBUG, "destroying virtual keyboard: %p", kbd);

    wl_list_remove(&kbd->destroy_l.link);

    cwc_keyboard_group_destroy(kbd->base);
    free(kbd);
}

/* vkbd keyboard group only has itself as the member */
static void on_new_vkbd(struct wl_listener *listener, void *data)
{
    struct wlr_virtual_keyboard_v1 *vkbd = data;
    struct cwc_seat *seat = vkbd->seat ? vkbd->seat->data : server.seat;
    struct cwc_keyboard_group *kbd_group =
        cwc_keyboard_group_create(seat, vkbd);
    cwc_keyboard_group_add_device(kbd_group, &vkbd->keyboard.base);

    cwc_log(WLR_DEBUG, "new virtual keyboard (%s): %p", seat->wlr_seat->name,
            kbd_group);

    struct cwc_virtual_keyboard *cwc_vkbd = calloc(1, sizeof(*cwc_vkbd));
    cwc_vkbd->base                        = kbd_group;

    cwc_vkbd->destroy_l.notify = on_vkbd_destroy;
    wl_signal_add(&vkbd->keyboard.base.events.destroy, &cwc_vkbd->destroy_l);
}

static void on_shortcuts_inhibitor_destroy(struct wl_listener *listener,
                                           void *data)
{
    struct wlr_keyboard_shortcuts_inhibitor_v1 *inhibitor = data;
    struct cwc_seat *seat = inhibitor->seat->data;

    cwc_log(CWC_DEBUG, "destroying shortcut inhibitor: %p", inhibitor);

    LISTEN_DESTROY(listener);

    if (inhibitor == seat->kbd_inhibitor)
        seat->kbd_inhibitor = NULL;
}

static void on_new_inhibitor(struct wl_listener *listener, void *data)
{
    struct wlr_keyboard_shortcuts_inhibitor_v1 *inhibitor = data;

    cwc_log(CWC_DEBUG, "new shortcut inhibitor: %p", inhibitor);

    wlr_keyboard_shortcuts_inhibitor_v1_activate(inhibitor);
    LISTEN_CREATE(&inhibitor->events.destroy, on_shortcuts_inhibitor_destroy);

    struct cwc_seat *seat = inhibitor->seat->data;
    if (inhibitor->surface == inhibitor->seat->keyboard_state.focused_surface)
        seat->kbd_inhibitor = inhibitor;
}

void setup_keyboard(struct cwc_input_manager *input_mgr)
{
    input_mgr->virtual_kbd_manager =
        wlr_virtual_keyboard_manager_v1_create(server.wl_display);
    input_mgr->new_vkbd_l.notify = on_new_vkbd;
    wl_signal_add(&input_mgr->virtual_kbd_manager->events.new_virtual_keyboard,
                  &input_mgr->new_vkbd_l);

    input_mgr->kbd_inhibit_manager =
        wlr_keyboard_shortcuts_inhibit_v1_create(server.wl_display);
    input_mgr->new_keyboard_inhibitor_l.notify = on_new_inhibitor;
    wl_signal_add(&input_mgr->kbd_inhibit_manager->events.new_inhibitor,
                  &input_mgr->new_keyboard_inhibitor_l);
}

void cleanup_keyboard(struct cwc_input_manager *input_mgr)
{
    wl_list_remove(&input_mgr->new_vkbd_l.link);

    wl_list_remove(&input_mgr->new_keyboard_inhibitor_l.link);
}
