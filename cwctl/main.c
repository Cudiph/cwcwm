/* main.c - cwctl entry point
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

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "cwc/ipc.h"
#include "cwctl.h"
#include "script-asset.h"

static char *help_txt =
    "Usage:\n"
    "  cwctl [options] [COMMAND]\n"
    "\n"
    "Options:\n"
    "  -h, --help       show this message\n"
    "  -s, --socket     path to cwc ipc socket\n"
    "  -c, --command    evaluate lua expression without entering repl\n"
    "  -f, --file       evaluate lua script from file\n"
    "\n"
    "Commands:\n"
    "  client    Get all client information\n"
    "  screen    Get all screen information\n"
    "  binds     Get all active keybinds information\n"
    "  help      Help about any command/subcommand\n"
    "\n"
    "Example:\n"
    "  cwc -s /tmp/cwc.sock -c 'return cwc.client.focused().title'\n"
    "  cwc -f ./show-all-client.lua\n"
    "  cwc screen\n"
    "  cwc -s /tmp/cwc.sock screen --filter 'DP-1' set enabled false";

static struct option long_options[] = {
    {"help",    NO_ARG, NULL, 'h'},
    {"socket",  ARG,    NULL, 's'},
    {"command", ARG,    NULL, 'c'},
    {"file",    ARG,    NULL, 'f'},
    {NULL,      0,      NULL, 0  },
};

static char *socket_path = NULL;
static char *input       = NULL;
static char *result      = NULL;
static int client_fd     = 0;

void repl(char *cmd)
{
    int cfd = client_fd;

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    if (cmd && cmd != input) {
        strncpy(input, cmd, BUFFER_SIZE);
    } else if (!cmd) {
        printf("cwc# ");
    }

    while (cmd || fgets(input, BUFFER_SIZE, stdin) != NULL) {
        if (strlen(input) == 1)
            goto new_prompt;

        int msg_size = ipc_create_message(result, BUFFER_SIZE, IPC_EVAL, input);
        send(cfd, result, msg_size, 0);

        int res_len = 0;
        enum cwc_ipc_opcode opcode;
        const char *ipc_body = NULL;
        do {
            res_len  = recv(cfd, result, BUFFER_SIZE, 0);
            ipc_body = ipc_get_body(result, &opcode);
        } while (opcode != IPC_EVAL_RESPONSE);

        if (res_len - HEADER_SIZE == 0) {
            printf("<empty>");
        } else {
            write(1, ipc_body, res_len - HEADER_SIZE);
        }

        putchar('\n');
        if (cmd)
            break;

    new_prompt:
        printf("cwc# ");
    }
}

static int object_command(int argc, char **argv)
{
    if (optind >= argc)
        return 1;

    char *command = argv[optind++];

    if (strcmp(command, "help") == 0) {
        puts(help_txt);
    } else if (strcmp(command, "screen") == 0) {
        return screen_cmd(argc, argv);
    } else if (strcmp(command, "client") == 0) {
        repl((char *)_cwctl_script_client_lua);
    } else if (strcmp(command, "binds") == 0) {
        repl((char *)_cwctl_script_binds_lua);
    } else {
        fprintf(
            stderr,
            "command %s not found, run 'cwctl --help' to show all command\n",
            command);
        return 1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    socket_path = getenv("CWC_SOCK");
    char *cmd   = NULL;
    char *file  = NULL;

    int c;
    while ((c = getopt_long(argc, argv, "+hs:c:f:", long_options, NULL)) != -1)
        switch (c) {
        case 's':
            socket_path = optarg;
            break;
        case 'h':
            puts(help_txt);
            return 0;
        case 'c':
            cmd = optarg;
            break;
        case 'f':
            file = optarg;
            break;
        default:
            puts(help_txt);
            return 1;
        }

    if (!socket_path) {
        fprintf(stderr, "Cannot determine socket path\n");
        return -1;
    }

    client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd < 0) {
        perror("cannot create client socket");
        return -1;
    }

    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("cannot connect to cwc socket");
        return -1;
    }

    input  = malloc(BUFFER_SIZE + 1);
    result = malloc(BUFFER_SIZE + 1);

    int any_error = object_command(argc, argv);

    if (!any_error)
        goto cleanup;

    if (!input || !result) {
        errno = ENOMEM;
        goto error_cleanup;
    }

    if (file) {
        FILE *fstream = fopen(file, "r");
        if (!fstream)
            goto error_cleanup;

        fread(input, 1, BUFFER_SIZE, fstream);
        fclose(fstream);
        cmd = input;
    }

    repl(cmd);
cleanup:
    close(client_fd);
    return 0;

error_cleanup:
    perror(NULL);
    close(client_fd);
    return 2;
}
