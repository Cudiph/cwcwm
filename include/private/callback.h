struct wl_listener;

void on_keyboard_focus_change(struct wl_listener *listener, void *data);

void on_pointer_focus_change(struct wl_listener *listener, void *data);
void on_request_set_cursor(struct wl_listener *listener, void *data);
