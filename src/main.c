/* main.c - cwc entry point
 *
 * Copyright (C) 2024 Dwi Asmoro Bangun <dwiaceromo@gmail.com>
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

#include <dlfcn.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wlr/util/log.h>

#include "cwc/config.h"
#include "cwc/luac.h"
#include "cwc/server.h"

static char *help_txt = "Usage:\n"
                        "  cwc [options]\n"
                        "\n"
                        "Options:\n"
                        "  -h, --help       show this message\n"
                        "  -v, --version    show version\n"
                        "  -c, --config     lua configuration file to use\n"
                        "  -s, --startup    startup command\n"
                        "  -l, --library    library directory search path\n"
                        "  -d, --debug      +increase debug verbosity level\n"
                        "\n"
                        "Example:\n"
                        "  cwc -c ~/test/rc.lua -dd";

#define ARG    1
#define NO_ARG 0
static struct option long_options[] = {
    {"help",    NO_ARG, NULL, 'h'},
    {"version", NO_ARG, NULL, 'v'},
    {"config",  ARG,    NULL, 'c'},
    {"startup", ARG,    NULL, 's'},
    {"library", ARG,    NULL, 'l'},
    {"debug",   NO_ARG, NULL, 'd'},
};

// globals
struct cwc_server server   = {0};
struct cwc_config g_config = {0};
bool lua_initial_load      = true;
char *config_path          = NULL;
char *library_path         = NULL;

/* entry point */
int main(int argc, char **argv)
{
    char *startup_cmd = NULL;
    int exit_value    = 0;
    char log_level    = WLR_ERROR;

    setenv("XDG_CURRENT_DESKTOP", "cwc", true);
    setenv("_JAVA_AWT_WM_NONREPARENTING", "1", true);

    int c;
    while ((c = getopt_long(argc, argv, "hvc:s:l:p:d", long_options, NULL))
           != -1)
        switch (c) {
        case 'd':
            log_level++;
            if (log_level > 3)
                log_level = 3;
            break;
        case 'v':
            printf("cwc v%s-%s\n", CWC_VERSION, CWC_GITHASH);
            return 0;
        case 'c':
            config_path = optarg;
            break;
        case 's':
            startup_cmd = optarg;
            break;
        case 'l':
            library_path = optarg;
            break;
        case 'h':
            puts(help_txt);
            return 0;
        default:
            puts(help_txt);
            return 1;
        }

    wlr_log_init(log_level, NULL);
    cwc_config_init();

    if ((exit_value = server_init(&server, config_path, library_path))) {
        goto shutdown;
    }

    if (startup_cmd)
        spawn_with_shell(startup_cmd);

    wl_display_run(server.wl_display);

shutdown:
    server_fini(&server);
    luaC_fini();
    return exit_value;
}
