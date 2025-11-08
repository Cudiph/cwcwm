/* process.c - process management
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

#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-util.h>

#include "cwc/config.h"
#include "cwc/process.h"
#include "cwc/server.h"
#include "cwc/util.h"
#include "lauxlib.h"
#include "lua.h"

enum sigpfd_byte {
    CWC_GRACEFUL = 1,
    CWC_SIGCHLD,
};

static int sigpfd[2]                  = {0};
static struct wl_list monitored_child = {0}; // struct spawn_obj.link

static void graceful_handler(int signum)
{
    char value[1] = {CWC_GRACEFUL};
    write(sigpfd[1], &value, 1);
}

static void sigchld_handler(int signum)
{
    char value[1] = {CWC_SIGCHLD};
    write(sigpfd[1], &value, 1);
}

static void _spawn_exit_callback_call(struct spawn_obj *obj, int exit_code)
{
    struct cwc_process_callback_info *info = obj->info;
    lua_State *L                           = g_config_get_lua_State();

    if (info->type == CWC_PROCESS_TYPE_LUA) {

        lua_rawgeti(L, LUA_REGISTRYINDEX, obj->info->luaref_exited);
        lua_pushnumber(L, exit_code);
        lua_pushnumber(L, obj->pid);
        lua_rawgeti(L, LUA_REGISTRYINDEX, info->luaref_data);

        if (lua_pcall(L, 3, 0, 0))
            cwc_log(CWC_ERROR, "error when executing spawn callback: %s",
                    lua_tostring(L, -1));
    } else {
        info->on_exited(obj, exit_code, info->data);
    }
}

static void free_spawn_obj(struct spawn_obj *obj)
{
    struct cwc_process_callback_info *info = obj->info;

    if (obj->info->type == CWC_PROCESS_TYPE_LUA) {
        lua_State *L = g_config_get_lua_State();
        luaL_unref(L, LUA_REGISTRYINDEX, info->luaref_ioready);
        luaL_unref(L, LUA_REGISTRYINDEX, info->luaref_exited);
        luaL_unref(L, LUA_REGISTRYINDEX, info->luaref_data);
    }

    wl_list_remove(&obj->link);
    free(info);
    free(obj);
}

static void process_dead_child()
{
    int exit_code;
    pid_t waited_pid = waitpid(-1, &exit_code, WNOHANG);
    exit_code        = WEXITSTATUS(exit_code);

    struct spawn_obj *obj, *obj_temp;
    wl_list_for_each_safe(obj, obj_temp, &monitored_child, link)
    {
        if (waited_pid != obj->pid)
            continue;

        _spawn_exit_callback_call(obj, exit_code);
        free_spawn_obj(obj);
    }
}

static int on_sigpfd_ready(int fd, uint32_t mask, void *data)
{
    assert(fd == sigpfd[0]);
    char value[1];
    read(fd, value, 1);

    switch (value[0]) {
    case CWC_SIGCHLD:
        process_dead_child();
        break;
    case CWC_GRACEFUL:
        wl_display_terminate(server.wl_display);
    default:
        break;
    }

    return 0;
}

void setup_process(struct cwc_server *s)
{
    /* prepare self pipe */
    pipe(sigpfd);
    wl_list_init(&monitored_child);

    int flags = fcntl(sigpfd[0], F_GETFL);
    assert(flags != -1);
    flags |= O_NONBLOCK;
    int fset = fcntl(sigpfd[0], F_SETFL, flags);
    assert(fset != -1);

    flags = fcntl(sigpfd[1], F_GETFL);
    assert(flags != -1);
    flags |= O_NONBLOCK;
    fset = fcntl(sigpfd[1], F_SETFL, flags);
    assert(fset != -1);

    /* signal handler */
    struct sigaction sigchld_act = {.sa_handler = sigchld_handler};
    sigaction(SIGCHLD, &sigchld_act, NULL);

    struct sigaction graceful_act = {.sa_handler = graceful_handler};
    sigaction(SIGINT, &graceful_act, NULL);
    sigaction(SIGTERM, &graceful_act, NULL);

    wl_event_loop_add_fd(s->wl_event_loop, sigpfd[0], WL_EVENT_READABLE,
                         on_sigpfd_ready, &sigpfd);
}

void cleanup_process(struct cwc_server *s)
{
    close(sigpfd[0]);
    close(sigpfd[1]);
}

void _spawn(void *data)
{
    struct wl_array *argvarr = data;
    char **argv              = argvarr->data;
    cwc_log(CWC_DEBUG, "spawning : %s", argv[0]);
    if (fork() == 0) {
        setsid();

        // fork again so that it reparent to init when the first fork exited
        if (fork() == 0) {
            execvp(argv[0], argv);
            cwc_log(CWC_ERROR, "spawn failed [%d]: %s", errno, argv[0]);
        }

        _exit(0);
    }

    // function has argvarr ownership, release it
    char **s;
    wl_array_for_each(s, argvarr)
    {
        free(*s);
    }
    wl_array_release(argvarr);
    free(argvarr);
}

void spawn(char **argv)
{
    struct wl_array *argvarr = malloc(sizeof(*argvarr));
    wl_array_init(argvarr);

    int i = 0;
    while (argv[i] != NULL) {
        char **elm = wl_array_add(argvarr, sizeof(char *));
        *elm       = strdup(argv[i]);
        i++;
    }
    char **elm = wl_array_add(argvarr, sizeof(char *));
    *elm       = NULL;

    wl_event_loop_add_idle(server.wl_event_loop, _spawn, argvarr);
}

static void _spawn_with_shell(void *data)
{
    char *command = data;
    cwc_log(CWC_DEBUG, "spawning with shell: %s", command);
    if (fork() == 0) {
        setsid();

        if (fork() == 0) {
            execl("/bin/sh", "/bin/sh", "-c", command, NULL);
            cwc_log(CWC_ERROR, "spawn with shell failed: %s", command);
        }

        _exit(0);
    }

    free(command);
}

void spawn_with_shell(const char *const command)
{
    wl_event_loop_add_idle(server.wl_event_loop, _spawn_with_shell,
                           strdup(command));
}

static void _spawn_io_callback_call(struct spawn_obj *obj,
                                    const char *outbuf,
                                    bool is_stdout)
{
    struct cwc_process_callback_info *info = obj->info;
    lua_State *L                           = g_config_get_lua_State();

    if (info->type == CWC_PROCESS_TYPE_LUA) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, info->luaref_ioready);

        if (is_stdout) {
            lua_pushstring(L, outbuf);
            lua_pushnil(L);
        } else {
            lua_pushnil(L);
            lua_pushstring(L, outbuf);
        }

        lua_pushnumber(L, obj->pid);
        lua_rawgeti(L, LUA_REGISTRYINDEX, info->luaref_data);
        if (lua_pcall(L, 4, 0, 0))
            cwc_log(CWC_ERROR, "error when executing spawn callback: %s",
                    lua_tostring(L, -1));
    } else {
        if (is_stdout)
            info->on_ioready(obj, outbuf, NULL, info->data);
        else
            info->on_ioready(obj, NULL, outbuf, info->data);
    }
}

static void process_stdfd(int fd, struct spawn_obj *obj, bool is_stdout)
{
    int ready_bytes;
    ioctl(fd, FIONREAD, &ready_bytes);

    if (ready_bytes == 0) {
        if (is_stdout)
            wl_event_source_remove(obj->out);
        else
            wl_event_source_remove(obj->err);
        return;
    }

    const int MAX_STACK = 2048;
    if (ready_bytes <= MAX_STACK) {
        char outbuf[MAX_STACK + 1];
        int red     = read(fd, outbuf, ready_bytes);
        outbuf[red] = 0;

        _spawn_io_callback_call(obj, outbuf, is_stdout);
    } else {
        char *outbuf = malloc(ready_bytes + 1);
        int red      = read(fd, outbuf, ready_bytes);
        outbuf[red]  = 0;

        _spawn_io_callback_call(obj, outbuf, is_stdout);
        free(outbuf);
    }
}

static int on_pipe_stdout(int fd, uint32_t mask, void *data)
{
    struct spawn_obj *obj = data;

    process_stdfd(fd, obj, true);

    return 0;
}

static int on_pipe_stderr(int fd, uint32_t mask, void *data)
{
    struct spawn_obj *obj = data;

    process_stdfd(fd, obj, false);

    return 0;
}

struct spawn_async_data {
    char *command;
    struct cwc_process_callback_info *info;
};

static void _spawn_with_shell_easy_async(void *data)
{
    struct spawn_async_data *userdata = data;
    char *command                     = userdata->command;

    struct spawn_obj *spawned = calloc(1, sizeof(*spawned));
    if (!spawned)
        goto cleanup;

    cwc_log(CWC_DEBUG, "spawning with shell: %s", command);

    int pipefd_out[2];
    int pipefd_err[2];
    pipe(pipefd_out);
    pipe(pipefd_err);

    pid_t childpid = fork();
    if (childpid == -1) {
        free(userdata->info);
        free(spawned);
        close(pipefd_out[0]);
        close(pipefd_err[0]);
        cwc_log(CWC_ERROR, "can't create child process");
        goto cleanup_fd;
    } else if (childpid == 0) {
        close(pipefd_out[0]);
        close(pipefd_err[0]);
        dup2(pipefd_out[1], STDOUT_FILENO);
        dup2(pipefd_err[1], STDERR_FILENO);

        execl("/bin/sh", "/bin/sh", "-c", command, NULL);
        cwc_log(CWC_ERROR, "spawn with shell failed: %s", command);

        _exit(0);
    }

    spawned->pid        = childpid;
    spawned->pipefd_out = pipefd_out[0];
    spawned->pipefd_err = pipefd_err[0];
    spawned->info       = userdata->info;
    spawned->out =
        wl_event_loop_add_fd(server.wl_event_loop, spawned->pipefd_out,
                             WL_EVENT_READABLE, on_pipe_stdout, spawned);
    spawned->err =
        wl_event_loop_add_fd(server.wl_event_loop, spawned->pipefd_err,
                             WL_EVENT_READABLE, on_pipe_stderr, spawned);

    wl_list_insert(&monitored_child, &spawned->link);

cleanup_fd:
    close(pipefd_out[1]);
    close(pipefd_err[1]);
cleanup:
    free(command);
}

void spawn_with_shell_easy_async(const char *const command,
                                 struct cwc_process_callback_info info)
{
    struct cwc_process_callback_info *info_dup = malloc(sizeof(*info_dup));
    if (!info_dup)
        return;
    *info_dup = info;

    struct spawn_async_data *userdata = malloc(sizeof(*userdata));
    if (!userdata)
        return free(info_dup);

    userdata->info    = info_dup;
    userdata->command = strdup(command);

    wl_event_loop_add_idle(server.wl_event_loop, _spawn_with_shell_easy_async,
                           userdata);
}
