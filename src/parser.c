#include "parser.h"

#include <string.h>

int parse_command(command_t* cmd, char* buffer, ssize_t len)
{
    cmd->argc = 0;

    /* Strip the trailing line ending(s) so they don't end up as part of
     * the last argument. */
    while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
    {
        buffer[--len] = '\0';
    }

    /* strtok_r (rather than strtok) so this stays safe even if a future
     * caller ever parses more than one line concurrently. */
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
