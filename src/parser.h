#ifndef PARSER_H
#define PARSER_H

#include <sys/types.h>

#define MAX_ARGS 32

typedef struct command
{
    char* argv[MAX_ARGS];
    int   argc;
} command_t;

int parse_command(command_t* cmd, char* buffer, ssize_t len);

#endif
