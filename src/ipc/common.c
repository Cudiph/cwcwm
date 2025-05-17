/* ipc/common.c - common functions for ipc message handling
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

#include <string.h>

#include "cwc/ipc.h"

int ipc_create_message_n(
    char *dest, int maxlen, enum cwc_ipc_opcode opcode, const char *body, int n)
{
    int num_written = HEADER_SIZE + n;

    if (num_written > maxlen)
        return -1;

    strcpy(dest, IPC_HEADER);
    char opcstr[4] = {'\n', opcode, '\n', '\x00'};
    strcat(dest, opcstr);
    strncat(dest, body, n);

    return num_written;
}

int ipc_create_message(char *dest,
                       int maxlen,
                       enum cwc_ipc_opcode opcode,
                       const char *body)
{
    return ipc_create_message_n(dest, maxlen, opcode, body, strlen(body));
}

bool check_header(const char *msg)
{
    int hlen = sizeof(IPC_HEADER);

    if (strncmp(IPC_HEADER, msg, hlen - 1) != 0)
        return false;

    if (msg[hlen - 1] == '\n' && msg[hlen + 1] == '\n')
        return true;

    return false;
}

const char *ipc_get_body(const char *msg, enum cwc_ipc_opcode *opcode)
{
    if (!check_header(msg))
        return NULL;

    int hlen = sizeof(IPC_HEADER);
    if (opcode) {
        *opcode = msg[hlen];
    }

    return &msg[hlen + 2];
}
