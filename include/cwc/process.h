#ifndef _CWC_PROCESS_H
#define _CWC_PROCESS_H

#include <fcntl.h>
#include <wayland-server-core.h>

enum cwc_process_type {
    CWC_PROCESS_TYPE_LUA,
    CWC_PROCESS_TYPE_C,
};

struct spawn_obj;

struct cwc_process_callback_info {
    enum cwc_process_type type;
    union {
        int luaref_ioready;
        void (*on_ioready)(struct spawn_obj *spawn_obj,
                           const char *out,
                           const char *err,
                           void *data);
    };
    union {
        int luaref_exited;
        void (*on_exited)(struct spawn_obj *spawn_obj,
                          int exit_code,
                          void *data);
    };
    union {
        void *data;
        int luaref_data;
    };
};

struct spawn_obj {
    struct wl_list link;
    pid_t pid;
    struct cwc_process_callback_info *info;

    struct {
        int pipefd_out, pipefd_err;
        struct wl_event_source *out, *err;
    } CWC_PRIVATE;
};

void spawn(char **argv);
void spawn_with_shell(const char *const command);
void spawn_with_shell_easy_async(const char *const command,
                                 struct cwc_process_callback_info info);

#endif // !_CWC_PROCESS_H
