#ifndef _CWC_TEXT_INPUT_H
#define _CWC_TEXT_INPUT_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_compositor.h>

#include "cwc/input/seat.h"

struct cwc_text_input {
    struct wl_list link; // struct cwc_seat.text_inputs
    struct wlr_text_input_v3 *wlr;

    struct wl_listener enable_l;
    struct wl_listener commit_l;
    struct wl_listener disable_l;
    struct wl_listener destroy_l;
};

struct cwc_input_method {
    struct wlr_input_method_v2 *wlr;

    struct wl_listener commit_l;
    struct wl_listener new_popup_l;
    struct wl_listener grab_keyboard_l;
    struct wl_listener destroy_l;
};

void text_input_try_focus_surface(struct cwc_seat *seat,
                                  struct wlr_surface *surface);

#endif // !_CWC_TEXT_INPUT_H
