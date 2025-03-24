/* text_input.c - text_input and input_method protocol
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
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/types/wlr_text_input_v3.h>

#include "cwc/input/seat.h"
#include "cwc/input/text_input.h"
#include "cwc/server.h"
#include "cwc/util.h"

static void send_im_state(struct cwc_input_method *im,
                          struct cwc_text_input *text_input)
{
    struct wlr_input_method_v2 *wlr_im       = im->wlr;
    struct wlr_text_input_v3 *wlr_text_input = text_input->wlr;

    if (wlr_text_input->active_features
        & WLR_TEXT_INPUT_V3_FEATURE_SURROUNDING_TEXT)
        wlr_input_method_v2_send_surrounding_text(
            wlr_im, wlr_text_input->current.surrounding.text,
            wlr_text_input->current.surrounding.cursor,
            wlr_text_input->current.surrounding.anchor);

    wlr_input_method_v2_send_text_change_cause(
        wlr_im, wlr_text_input->current.text_change_cause);

    if (wlr_text_input->active_features
        & WLR_TEXT_INPUT_V3_FEATURE_CONTENT_TYPE)
        wlr_input_method_v2_send_content_type(
            wlr_im, wlr_text_input->current.content_type.hint,
            wlr_text_input->current.content_type.purpose);

    wlr_input_method_v2_send_done(im->wlr);
}

static void on_text_input_enable(struct wl_listener *listener, void *data)
{
    struct cwc_text_input *text_input =
        wl_container_of(listener, text_input, enable_l);
    struct cwc_seat *seat = text_input->wlr->seat->data;

    if (!seat->input_method)
        return;

    wlr_input_method_v2_send_activate(seat->input_method->wlr);
    send_im_state(seat->input_method, text_input);
}

static void on_text_input_commit(struct wl_listener *listener, void *data)
{
    struct cwc_text_input *text_input =
        wl_container_of(listener, text_input, commit_l);
    struct cwc_seat *seat = text_input->wlr->seat->data;

    if (!seat->input_method)
        return;

    send_im_state(seat->input_method, text_input);
}

static void on_text_input_disable(struct wl_listener *listener, void *data)
{
    struct cwc_text_input *text_input =
        wl_container_of(listener, text_input, disable_l);
    struct cwc_seat *seat = text_input->wlr->seat->data;

    if (!seat->input_method)
        return;

    wlr_input_method_v2_send_deactivate(seat->input_method->wlr);
    send_im_state(seat->input_method, text_input);
}

static void on_text_input_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_text_input *text_input =
        wl_container_of(listener, text_input, destroy_l);
    struct cwc_seat *seat = text_input->wlr->seat->data;

    if (seat->focused_text_input == text_input)
        seat->focused_text_input = NULL;

    cwc_log(CWC_DEBUG, "destroying text input: %p", text_input);

    wl_list_remove(&text_input->link);

    wl_list_remove(&text_input->enable_l.link);
    wl_list_remove(&text_input->commit_l.link);
    wl_list_remove(&text_input->disable_l.link);
    wl_list_remove(&text_input->destroy_l.link);

    free(text_input);
}

static void on_new_text_input(struct wl_listener *listener, void *data)
{
    struct wlr_text_input_v3 *wlr_text_input = data;
    struct cwc_seat *seat                    = wlr_text_input->seat->data;

    struct cwc_text_input *text_input = calloc(1, sizeof(*text_input));
    if (!text_input)
        return;

    cwc_log(CWC_DEBUG, "creating text input: %p", text_input);

    text_input->wlr = wlr_text_input;

    text_input->enable_l.notify  = on_text_input_enable;
    text_input->commit_l.notify  = on_text_input_commit;
    text_input->disable_l.notify = on_text_input_disable;
    text_input->destroy_l.notify = on_text_input_destroy;
    wl_signal_add(&wlr_text_input->events.enable, &text_input->enable_l);
    wl_signal_add(&wlr_text_input->events.commit, &text_input->commit_l);
    wl_signal_add(&wlr_text_input->events.disable, &text_input->disable_l);
    wl_signal_add(&wlr_text_input->events.destroy, &text_input->destroy_l);

    wl_list_insert(&seat->text_inputs, &text_input->link);
}

static void on_input_method_commit(struct wl_listener *listener, void *data)
{
    struct cwc_input_method *im    = wl_container_of(listener, im, commit_l);
    struct cwc_seat *seat          = im->wlr->seat->data;
    struct cwc_text_input *focused = seat->focused_text_input;

    if (!focused)
        return;

    struct wlr_input_method_v2 *ctx = data;
    if (ctx->current.preedit.text)
        wlr_text_input_v3_send_preedit_string(
            focused->wlr, ctx->current.preedit.text,
            ctx->current.preedit.cursor_begin, ctx->current.preedit.cursor_end);

    if (ctx->current.commit_text)
        wlr_text_input_v3_send_commit_string(focused->wlr,
                                             ctx->current.commit_text);

    if (ctx->current.delete.before_length || ctx->current.delete.after_length)
        wlr_text_input_v3_send_delete_surrounding_text(
            focused->wlr, ctx->current.delete.before_length,
            ctx->current.delete.after_length);

    wlr_text_input_v3_send_done(focused->wlr);
}

static void on_input_method_new_popup(struct wl_listener *listener, void *data)
{
    struct cwc_input_method *im = wl_container_of(listener, im, new_popup_l);

    // TODO: handle popup
}

static void on_kbd_grab_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_seat *seat = wl_container_of(listener, seat, kbd_grab_destroy_l);

    seat->kbd_grab = NULL;

    wl_list_remove(&seat->kbd_grab_destroy_l.link);
}

static void on_input_method_grab_kbd(struct wl_listener *listener, void *data)
{
    struct cwc_input_method *im =
        wl_container_of(listener, im, grab_keyboard_l);
    struct cwc_seat *seat                           = im->wlr->seat->data;
    struct wlr_input_method_keyboard_grab_v2 *event = data;

    if (seat->kbd_grab) {
        wlr_input_method_keyboard_grab_v2_destroy(event);
        return;
    }

    seat->kbd_grab = event;

    wlr_input_method_keyboard_grab_v2_set_keyboard(
        event, wlr_seat_get_keyboard(seat->wlr_seat));

    seat->kbd_grab_destroy_l.notify = on_kbd_grab_destroy;
    wl_signal_add(&event->events.destroy, &seat->kbd_grab_destroy_l);
}

static void on_input_method_destroy(struct wl_listener *listener, void *data)
{
    struct cwc_input_method *im = wl_container_of(listener, im, destroy_l);
    struct cwc_seat *seat       = im->wlr->seat->data;
    seat->input_method          = NULL;

    cwc_log(CWC_DEBUG, "destroying input method: %p", im);

    wl_list_remove(&im->commit_l.link);
    wl_list_remove(&im->new_popup_l.link);
    wl_list_remove(&im->grab_keyboard_l.link);
    wl_list_remove(&im->destroy_l.link);

    free(im);
}

static void on_new_input_method(struct wl_listener *listener, void *data)
{
    struct wlr_input_method_v2 *wlr_input_method = data;
    struct cwc_seat *seat                        = wlr_input_method->seat->data;
    if (seat->input_method) {
        wlr_input_method_v2_send_unavailable(wlr_input_method);
        return;
    }

    struct cwc_input_method *input_method = calloc(1, sizeof(*input_method));
    if (!input_method) {
        wlr_input_method_v2_send_unavailable(wlr_input_method);
        return;
    }

    cwc_log(CWC_DEBUG, "creating input method: %p", wlr_input_method);

    seat->input_method = input_method;
    input_method->wlr  = wlr_input_method;

    input_method->commit_l.notify        = on_input_method_commit;
    input_method->new_popup_l.notify     = on_input_method_new_popup;
    input_method->grab_keyboard_l.notify = on_input_method_grab_kbd;
    input_method->destroy_l.notify       = on_input_method_destroy;
    wl_signal_add(&wlr_input_method->events.commit, &input_method->commit_l);
    wl_signal_add(&wlr_input_method->events.new_popup_surface,
                  &input_method->new_popup_l);
    wl_signal_add(&wlr_input_method->events.grab_keyboard,
                  &input_method->grab_keyboard_l);
    wl_signal_add(&wlr_input_method->events.destroy, &input_method->destroy_l);

    if (!seat->focused_text_input)
        text_input_try_focus_surface(
            seat, seat->wlr_seat->keyboard_state.focused_surface);
}

void setup_text_input(struct cwc_server *s)
{
    s->text_input_manager = wlr_text_input_manager_v3_create(s->wl_display);
    s->new_text_input_l.notify = on_new_text_input;
    wl_signal_add(&s->text_input_manager->events.text_input,
                  &s->new_text_input_l);

    s->input_method_manager = wlr_input_method_manager_v2_create(s->wl_display);
    s->new_input_method_l.notify = on_new_input_method;
    wl_signal_add(&s->input_method_manager->events.input_method,
                  &s->new_input_method_l);
}

void cleanup_text_input(struct cwc_server *s)
{
    wl_list_remove(&s->new_text_input_l.link);
    wl_list_remove(&s->new_input_method_l.link);
}

void text_input_try_focus_surface(struct cwc_seat *seat,
                                  struct wlr_surface *surface)
{
    struct cwc_text_input *text_input;
    wl_list_for_each(text_input, &seat->text_inputs, link)
    {
        if (surface
            && wl_resource_get_client(text_input->wlr->resource)
                   == wl_resource_get_client(surface->resource)) {
            wlr_text_input_v3_send_enter(text_input->wlr, surface);
            seat->focused_text_input = text_input;
        } else if (text_input->wlr->focused_surface) {
            if (seat->input_method) {
                wlr_input_method_v2_send_deactivate(seat->input_method->wlr);
                send_im_state(seat->input_method, text_input);
            }
            wlr_text_input_v3_send_leave(text_input->wlr);
        }
    }
}
