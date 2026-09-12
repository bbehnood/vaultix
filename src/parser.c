#include "parser.h"

#include <string.h>

int parse_command(command_t* cmd, char* buffer, ssize_t len)
{
    cmd->argc = 0;

    while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
    {
        buffer[--len] = '\0';
    }

    char* saveptr;
    char* tok = strtok_r(buffer, " \t", &saveptr);

    while (tok)
    {
        if (cmd->argc == MAX_ARGS)
        {
            return -1;
        }

        cmd->argv[cmd->argc++] = tok;
        tok                    = strtok_r(NULL, " \t", &saveptr);
    }

    return cmd->argc;
}
