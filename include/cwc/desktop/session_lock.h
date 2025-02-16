#ifndef _CWC_SESSION_LOCK
#define _CWC_SESSION_LOCK

#include <wayland-server-core.h>
#include <wlr/types/wlr_session_lock_v1.h>

struct cwc_session_locker;

struct cwc_session_lock_manager {
    struct cwc_server *server;
    struct wlr_session_lock_manager_v1 *manager;
    bool locked;
    struct cwc_session_locker *locker; // available when locked = true

    struct wl_listener new_lock_l;
    struct wl_listener destroy_l;
};

struct cwc_session_locker {
    struct wlr_session_lock_v1 *locker;
    struct cwc_session_lock_manager *manager;

    struct wl_listener unlock_l;
    struct wl_listener new_surface_l;
    struct wl_listener destroy_l;
};

#endif // !_CWC_SESSION_LOCK
