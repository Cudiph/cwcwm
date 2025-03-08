/* ipc/server.c - unix socket ipc
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

#include <fcntl.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-util.h>

#include "cwc/config.h"
#include "cwc/ipc.h"
#include "cwc/server.h"
#include "cwc/util.h"
#include "lauxlib.h"

#define BUFFER_SIZE 1e6

struct wl_list client_list;
struct ipc_client {
    struct wl_list link; // client_list
    int fd;
    struct wl_event_source *event_source;
};

static void ipc_client_close(struct ipc_client *c)
{
    cwc_log(CWC_DEBUG, "closing ipc connection for fd: %d", c->fd);
    shutdown(c->fd, SHUT_RDWR);
    close(c->fd);

    wl_list_remove(&c->link);
    wl_event_source_remove(c->event_source);

    free(c);
}

static void handle_eval_msg(int fd, char *msg_buffer)
{
    const char *body = ipc_get_body(msg_buffer, NULL);

    lua_State *L        = g_config_get_lua_State();
    size_t returned_len = 0;
    int stack_size      = lua_gettop(L);
    int error           = luaL_dostring(L, body);
    if (error) {
        cwc_log(CWC_ERROR, "%s", lua_tostring(L, -1));
        const char *dostring_res = lua_tolstring(L, -1, &returned_len);
        returned_len =
            ipc_create_message_n(msg_buffer, BUFFER_SIZE, IPC_EVAL_RESPONSE,
                                 dostring_res, returned_len);
    } else if (stack_size != lua_gettop(L)) {
        lua_getglobal(L, "tostring");
        lua_pushvalue(L, -2);
        if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
            cwc_log(CWC_ERROR, "%s", lua_tostring(L, -1));
            goto send_response;
        }

        const char *dostring_res = lua_tolstring(L, -1, &returned_len);
        returned_len =
            ipc_create_message_n(msg_buffer, BUFFER_SIZE, IPC_EVAL_RESPONSE,
                                 dostring_res, returned_len);
    } else {
        returned_len = ipc_create_message_n(msg_buffer, BUFFER_SIZE,
                                            IPC_EVAL_RESPONSE, "", 0);
    }

send_response:
    send(fd, msg_buffer, returned_len, 0);
}

static int ipc_handle_client_msg(int fd, uint32_t mask, void *data)
{
    struct ipc_client *c = data;
    if (mask & WL_EVENT_ERROR || mask & WL_EVENT_HANGUP) {
        ipc_client_close(c);
        return 0;
    }

    char *msg_buffer     = malloc(BUFFER_SIZE + 1);
    int read_len         = read(fd, msg_buffer, BUFFER_SIZE);
    msg_buffer[read_len] = 0;

    enum cwc_ipc_opcode opcode;
    const char *body = ipc_get_body(msg_buffer, &opcode);
    if (!body) {
        cwc_log(CWC_ERROR, "ipc message error: %s", msg_buffer);
        goto free_and_exit;
    }

    switch (opcode) {
    case IPC_EVAL:
        handle_eval_msg(fd, msg_buffer);
        break;
    default:
        break;
    }

free_and_exit:
    free(msg_buffer);
    return 0;
}

static int ipc_handle_new_conn(int fd, uint32_t mask, void *data)
{
    struct cwc_server *s = data;
    int client_fd        = accept(fd, NULL, NULL);

    if (client_fd < 0)
        return 0;

    struct ipc_client *c = calloc(1, sizeof(*c));
    if (!c) {
        close(client_fd);
        return 0;
    }

    cwc_log(CWC_DEBUG, "new ipc connection with fd: %d", client_fd);

    c->fd = client_fd;
    c->event_source =
        wl_event_loop_add_fd(s->wl_event_loop, client_fd, WL_EVENT_READABLE,
                             ipc_handle_client_msg, c);

    wl_list_insert(&client_list, &c->link);

    return 0;
}

void setup_ipc(struct cwc_server *s)
{
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    int path_len            = sizeof(addr.sun_path) - 1;
    int result_len = snprintf(addr.sun_path, path_len, "%s/cwc.%d.%d.sock",
                              getenv("XDG_RUNTIME_DIR"), getuid(), getpid());

    wl_list_init(&client_list);

    if (result_len >= path_len) {
        cwc_log(CWC_ERROR, "socket path to long");
        return;
    }

    if ((s->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        cwc_log(CWC_ERROR, "failed to create socket (%s)", addr.sun_path);
        perror("ipc");
        return;
    }

    if (fcntl(s->socket_fd, F_SETFD, FD_CLOEXEC)) {
        cwc_log(CWC_ERROR, "failed to set FD_CLOEXEC");
        perror("ipc");
        return;
    }

    if (fcntl(s->socket_fd, F_SETFL, O_NONBLOCK) == -1) {
        cwc_log(CWC_ERROR, "failed to set O_NONBLOCK");
        perror("ipc");
        return;
    }

    unlink(addr.sun_path);
    if (bind(s->socket_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        cwc_log(CWC_ERROR, "failed to bind ipc socket");
        perror("ipc");
        return;
    }

    if (listen(s->socket_fd, 7) == -1) {
        cwc_log(CWC_ERROR, "socket failed to listen");
        perror("ipc");
        return;
    }

    s->socket_path = strdup(addr.sun_path);
    setenv("CWC_SOCK", addr.sun_path, true);

    wl_event_loop_add_fd(s->wl_event_loop, s->socket_fd, WL_EVENT_READABLE,
                         ipc_handle_new_conn, s);
}

void cleanup_ipc(struct cwc_server *s)
{
    struct ipc_client *c;
    wl_list_for_each(c, &client_list, link)
    {
        ipc_client_close(c);
    }

    if (!s->socket_fd)
        return;

    close(s->socket_fd);
    unlink(s->socket_path);
    unsetenv("CWC_SOCK");
    free(s->socket_path);
    s->socket_path = NULL;
}
