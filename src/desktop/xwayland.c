#ifndef CWC_XWAYLAND

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>

#include "cwc/server.h"
#include "cwc/util.h"
#include "private/server.h"

static int on_x11_socket_fd(int fd, uint32_t mask, void *data);
static int on_xwayland_satellite_exit(int fd, uint32_t mask, void *data);

static int xwayland_satellite_binary_exists(void)
{
    const char *path_env = getenv("PATH");
    if (!path_env)
        return false;

    char *path = strdup(path_env);
    if (!path)
        return false;

    char *dir = strtok(path, ":");
    char full_path[256];

    while (dir) {
        snprintf(full_path, sizeof(full_path), "%s/xwayland-satellite", dir);
        if (access(full_path, X_OK) == 0) {
            free(path);
            return true;
        }
        dir = strtok(NULL, ":");
    }

    free(path);
    return false;
}

static int bind_socket(struct sockaddr_un *addr, size_t size)
{
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;

    if (bind(fd, (struct sockaddr *)addr, size) == 0 && listen(fd, 4096) == 0) {
        return fd;
    }
    close(fd);
    return -1;
}

static bool open_x11_sockets(int *display, int *x11_socket_fd, int *x11_abs_socket_fd)
{
    char lock_path[64];
    int unix_fd = -1;     // unix socket
    int abstract_fd = -1; // abstract socket

    for (int d = 0; d <= 32; d++) {
        snprintf(lock_path, sizeof(lock_path), "/tmp/.X%d-lock", d);

        // try to create the lockfile
        int lock_fd = open(lock_path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0444);
        if (lock_fd < 0)
            continue;

        // write PID to lock file
        char pid_str[12];
        int len = snprintf(pid_str, sizeof(pid_str), "%10d\n", getpid());
        if (write(lock_fd, pid_str, len) != len){
            close(lock_fd);
            unlink(lock_path);
            continue;
        }
        close(lock_fd);

        // prepare the sockets
        struct sockaddr_un addr, abs_addr;
        memset(&addr, 0, sizeof(addr));
        memset(&abs_addr, 0, sizeof(abs_addr));
        addr.sun_family = AF_UNIX;
        abs_addr.sun_family = AF_UNIX;

        const char *dir = "/tmp/.X11-unix";
        struct stat st;
        // create socket directory if not exists
        if (stat(dir, &st) != 0)
            mkdir("/tmp/.X11-unix", 01777);

        // unix socket path
        snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/.X11-unix/X%d", d);
        unlink(addr.sun_path); // remove any stale socket file

        // abstract socket path
        // the first byte must be null '\0' for abstract sockets
        abs_addr.sun_path[0] = 0;
        snprintf(abs_addr.sun_path + 1, sizeof(addr.sun_path) - 1, "/tmp/.X11-unix/X%d", d);
        // calculate the exact size: family + null byte + string length
        size_t abs_len = offsetof(struct sockaddr_un, sun_path) + 1 + strlen(abs_addr.sun_path + 1);

        unix_fd = bind_socket(&addr, sizeof(addr));
        abstract_fd = bind_socket(&abs_addr, abs_len);

        // skip if any socket fails to bind
        if (unix_fd < 0 || abstract_fd < 0) {
            if (unix_fd != -1) {
                close(unix_fd);
                unlink(addr.sun_path);
            }
            if (abstract_fd != -1)
                close(abstract_fd);

            unlink(lock_path);
            continue;
        }

        // success! we have both sockets
        *display = d;
        *x11_socket_fd = unix_fd;
        *x11_abs_socket_fd = abstract_fd;
        return true;
    }

    return false;
}

static int on_x11_socket_fd(int fd, uint32_t mask, void *data)
{
    struct cwc_server *server = data;

    if (server->xwayland_satellite_pid)
        return 0;

    // lazy spawn: received a connection on the X11 sockets, so start the satellite
    pid_t pid = fork();
    if (pid < 0)
        return 0;

    if (pid == 0) {
        // --- CHILD ---
        // restore default signal handlers
        sigset_t set;
        sigemptyset(&set);
        sigprocmask(SIG_SETMASK, &set, NULL);

        fcntl(server->x11_socket_fd, F_SETFD, 0); // Allow satellite to inherit FD
        fcntl(server->x11_abs_socket_fd, F_SETFD, 0);

        char fd1_str[16], fd2_str[16], disp_str[16];
        snprintf(fd1_str, sizeof(fd1_str), "%d", server->x11_socket_fd);
        snprintf(fd2_str, sizeof(fd2_str), "%d", server->x11_abs_socket_fd);
        snprintf(disp_str, sizeof(disp_str), ":%d", server->x11_display);

        char *argv[] = {"xwayland-satellite", disp_str, "-listenfd", fd1_str,
                                            "-listenfd", fd2_str, NULL};
        execvp("xwayland-satellite", argv);
        _exit(1); /* exec failed */
    } else {
        // --- PARENT ---
        server->xwayland_satellite_pid = pid;
        server->xwayland_satellite_pidfd = syscall(SYS_pidfd_open, pid, 0);

        // stop listening to the sockets after starting satellite
        if (server->x11_fd_source) {
            wl_event_source_remove(server->x11_fd_source);
            server->x11_fd_source = NULL;
        }
        if (server->x11_abs_fd_source) {
            wl_event_source_remove(server->x11_abs_fd_source);
            server->x11_abs_fd_source = NULL;
        }

        if (server->xwayland_satellite_pidfd != -1) {
            server->xwayland_satellite_exit_source = wl_event_loop_add_fd(server->wl_event_loop,
                                                server->xwayland_satellite_pidfd,
                                                WL_EVENT_READABLE, on_xwayland_satellite_exit, server);
        } else {
            cwc_log(CWC_ERROR, "pidfd_open failed. xwayland-satellite integration requires kernel 5.3 or later");
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            server->xwayland_satellite_pid = 0;
            return 0;
        }

        if (!server->xwayland_satellite_exit_source) {
            cwc_log(CWC_ERROR, "failed to add pidfd event source for satellite process");
            close(server->xwayland_satellite_pidfd);
            server->xwayland_satellite_pidfd = -1;
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            server->xwayland_satellite_pid = 0;
            return 0;
        }
    }

    return 0;
}

static int on_xwayland_satellite_exit(int fd, uint32_t mask, void *data)
{
    int status;

    struct cwc_server *server = data;
    pid_t saved_pid = server->xwayland_satellite_pid;

    waitpid(saved_pid, &status, WNOHANG);
    cwc_log(CWC_INFO, "xwayland-satellite (pid %d) exited with status %d", saved_pid, status);

    if (server->xwayland_satellite_exit_source) {
        wl_event_source_remove(server->xwayland_satellite_exit_source);
        server->xwayland_satellite_exit_source = NULL;
    }
    if (server->xwayland_satellite_pidfd != -1) {
        close(server->xwayland_satellite_pidfd);
        server->xwayland_satellite_pidfd = -1;
    }

    server->xwayland_satellite_pid = 0;

    // re-register the listeners so cwc can catch the next X11 app
    server->x11_fd_source = wl_event_loop_add_fd(server->wl_event_loop, server->x11_socket_fd,
                                        WL_EVENT_READABLE, on_x11_socket_fd, server);
    server->x11_abs_fd_source = wl_event_loop_add_fd(server->wl_event_loop, server->x11_abs_socket_fd,
                                        WL_EVENT_READABLE, on_x11_socket_fd, server);

    return 0;
}

void xwayland_satellite_init(struct cwc_server *server)
{
    server->x11_display = -1;
    server->x11_socket_fd = -1;
    server->x11_abs_socket_fd = -1;
    server->xwayland_satellite_pid = 0;
    server->xwayland_satellite_pidfd = -1;
    server->xwayland_satellite_exit_source = NULL;
    server->x11_fd_source = NULL;
    server->x11_abs_fd_source = NULL;

    if (!xwayland_satellite_binary_exists()) {
        cwc_log(CWC_INFO, "xwayland-satellite binary not found; skipping integration");
        return;
    }

    if (server->x11_socket_fd == -1) {
        if (!open_x11_sockets(&server->x11_display, &server->x11_socket_fd, &server->x11_abs_socket_fd)) {
            cwc_log(CWC_ERROR, "failed to open X11 sockets");
            return;
        }

        char disp_env[16];
        snprintf(disp_env, sizeof(disp_env), ":%d", server->x11_display);
        setenv("DISPLAY", disp_env, 1);
        cwc_log(CWC_INFO, "X11 bridge ready on DISPLAY %s", disp_env);
    }

    // monitor the X11 sockets. When a client connects, we spawn the satellite.
    // spawn xwayland-satellite when connection received at any of the sockets
    server->x11_fd_source = wl_event_loop_add_fd(server->wl_event_loop, server->x11_socket_fd,
                                        WL_EVENT_READABLE, on_x11_socket_fd, server);
    server->x11_abs_fd_source = wl_event_loop_add_fd(server->wl_event_loop, server->x11_abs_socket_fd,
                                        WL_EVENT_READABLE, on_x11_socket_fd, server);
}

void xwayland_satellite_fini(struct cwc_server *server)
{
    char path[128];

    /* remove any event sources first */
    if (server->x11_fd_source) {
        wl_event_source_remove(server->x11_fd_source);
        server->x11_fd_source = NULL;
    }
    if (server->x11_abs_fd_source) {
        wl_event_source_remove(server->x11_abs_fd_source);
        server->x11_abs_fd_source = NULL;
    }
    if (server->xwayland_satellite_exit_source) {
        wl_event_source_remove(server->xwayland_satellite_exit_source);
        server->xwayland_satellite_exit_source = NULL;
    }

    /* try to reap the child if present. */
    if (server->xwayland_satellite_pid > 0) {
        pid_t saved_pid = server->xwayland_satellite_pid;
        int status = 0;
        pid_t r;

        /* try non-blocking reap */
        do {
            r = waitpid(saved_pid, &status, WNOHANG);
        } while (r == -1 && errno == EINTR);

        if (r == 0) {
            /* child still running: try to terminate gracefully and block */
            kill(saved_pid, SIGTERM);
            do {
                r = waitpid(saved_pid, &status, 0);
            } while (r == -1 && errno == EINTR);
        }
        server->xwayland_satellite_pid = 0;
    }

    /* close pidfd */
    if (server->xwayland_satellite_pidfd != -1) {
        close(server->xwayland_satellite_pidfd);
        server->xwayland_satellite_pidfd = -1;
    }

    /* close the listening sockets */
    if (server->x11_socket_fd != -1) {
        close(server->x11_socket_fd);
        server->x11_socket_fd = -1;
    }
    if (server->x11_abs_socket_fd != -1) {
        close(server->x11_abs_socket_fd);
        server->x11_abs_socket_fd = -1;
    }

    /* remove socket files */
    snprintf(path, sizeof(path), "/tmp/.X11-unix/X%d", server->x11_display);
    unlink(path);
    snprintf(path, sizeof(path), "/tmp/.X%d-lock", server->x11_display);
    unlink(path);

    cwc_log(CWC_INFO, "X11 bridge closed...");
}

#endif // CWC_XWAYLAND
