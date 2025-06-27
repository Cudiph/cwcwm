/* cmd-screen.c - cwctl screen command
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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cwctl.h"
#include "script-asset.h"

static char *screen_help =
    "Manage screen object\n"
    "\n"
    "Usage\n"
    "  cwctl screen [options] [COMMAND [ARG...]]\n"
    "\n"
    "Available Commands:\n"
    "  list\n"
    "       list all screen information\n"
    "\n"
    "  toggle <PROPERTY>\n"
    "       toggle screen property with boolean data type\n"
    "\n"
    "  set <PROPERTY> <VALUE>\n"
    "       set the property of a screen\n"
    "\n"
    "  get <PROPERTY>\n"
    "       get the property of a screen\n"
    "\n"
    "Options:\n"
    "  -h, --help   get the property of a screen\n"
    "  -f, --filter specify which output name for get,set,toggle to apply. "
    "default is 'focused' output, use '*' for every output\n"
    "\n"
    "Example:\n"
    "  cwctl screen -f 'eDP-1' set enabled false\n";
;

static struct option screen_long_opt[] = {
    {"help",   NO_ARG, NULL, 'h'},
    {"filter", ARG,    NULL, 'f'},
    {NULL,     0,      NULL, 0  },
};

static char *filter        = "focused";
static char formatted[100] = {0};

static void handle_list(char *script)
{
    strcat(script, "return scr_list()");
    repl(script);
}

static void handle_toggle(int cmd_argcount, int argc, char **argv, char *script)
{
    if (cmd_argcount < 1) {
        fprintf(stderr, "missing property argument\n");
        return;
    }

    snprintf(formatted, 99, "return scr_set('%s', '%s', 'toggle')\n", filter,
             argv[optind + 1]);
    strcat(script, formatted);
    repl(script);
}

static void handle_set(int cmd_argcount, int argc, char **argv, char *script)
{
    if (cmd_argcount < 2) {
        fprintf(stderr, "not enough argument\n");
        return;
    }

    snprintf(formatted, 99, "return scr_set('%s', '%s', %s)\n", filter,
             argv[optind + 1], argv[optind + 2]);
    strcat(script, formatted);
    repl(script);
}

static void handle_get(int cmd_argcount, int argc, char **argv, char *script)
{
    if (cmd_argcount < 1) {
        fprintf(stderr, "missing property argument\n");
        return;
    }

    snprintf(formatted, 99, "return scr_get('%s', '%s')\n", filter,
             argv[optind + 1]);
    strcat(script, formatted);
    repl(script);
}

int screen_cmd(int argc, char **argv)
{
    char *script = calloc(1, _cwctl_script_screen_lua_len + 100);
    strcpy(script, (char *)_cwctl_script_screen_lua);

    int c;
    while ((c = getopt_long(argc, argv, "hf:", screen_long_opt, NULL)) != -1)
        switch (c) {
        case 'h':
            puts(screen_help);
            goto cleanup;
        case 'f':
            filter = optarg;
            break;
        default:
            puts(screen_help);
            goto cleanup;
        }

    if ((argc - optind) == 0) {
        handle_list(script);
        goto cleanup;
    }

    char *command    = argv[optind];
    int cmd_argcount = argc - optind - 1;

    if (strcmp(command, "list") == 0) {
        handle_list(script);
    } else if (strcmp(command, "toggle") == 0) {
        handle_toggle(cmd_argcount, argc, argv, script);
    } else if (strcmp(command, "set") == 0) {
        handle_set(cmd_argcount, argc, argv, script);
    } else if (strcmp(command, "get") == 0) {
        handle_get(cmd_argcount, argc, argv, script);
    } else if (strcmp(command, "help") == 0) {
        puts(screen_help);
    } else {
        fprintf(stderr,
                "command %s not found, run 'cwctl screen --help' to show all "
                "command\n",
                command);
        return 1;
    }

cleanup:
    free(script);
    return 0;
}
