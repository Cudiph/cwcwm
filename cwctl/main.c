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

static char *help_txt =
    "Usage:\n"
    "  cwctl [options]\n"
    "\n"
    "Options:\n"
    "  -h, --help       show this message\n"
    "  -s, --socket     path to cwc ipc socket\n"
    "  -c, --command    evaluate lua expression without entering repl\n"
    "  -f, --file       evaluate lua script from file\n"
    "\n"
    "Example:\n"
    "  cwc -s /tmp/cwc.sock -c 'return cwc.client.focused().title'\n"
    "  cwc -f ./show-all-client.lua";

#define ARG    1
#define NO_ARG 0
static struct option long_options[] = {
    {"help",    NO_ARG, NULL, 'h'},
    {"socket",  ARG,    NULL, 's'},
    {"command", ARG,    NULL, 'c'},
    {"file",    ARG,    NULL, 'f'},
};

static char *socket_path = NULL;
static char *input       = NULL;
static char *result      = NULL;

#define BUFFER_SIZE 1e6
static void repl(int cfd, char *cmd)
{
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

int main(int argc, char **argv)
{
    socket_path = getenv("CWC_SOCK");
    char *cmd   = NULL;
    char *file  = NULL;

    int c;
    while ((c = getopt_long(argc, argv, "hs:c:f:", long_options, NULL)) != -1)
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

    int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
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

    repl(client_fd, cmd);
    close(client_fd);
    return 0;

error_cleanup:
    perror(NULL);
    close(client_fd);
    return 2;
}
