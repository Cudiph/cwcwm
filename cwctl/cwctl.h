#ifndef _CWCTL_H
#define _CWCTL_H

#define ARG         1
#define NO_ARG      0
#define BUFFER_SIZE 1e6

void repl(char *cmd);

int screen_cmd(int argc, char **argv);

#endif // !_CWCTL_H
