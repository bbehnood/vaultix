#ifndef PARSER_H
#define PARSER_H

#include <sys/types.h>

#define MAX_ARGS 32

/*
 * A parsed command line. `argv` entries point into the caller's buffer
 * (see parse_command) rather than owning their own storage.
 */
typedef struct command
{
    char* argv[MAX_ARGS];
    int   argc;
} command_t;

/*
 * Tokenizes `buffer` (a single command line, of `len` bytes, optionally
 * '\n'/'\r\n'-terminated) into `cmd` by splitting on spaces/tabs.
 *
 * Destructive: this writes NUL terminators into `buffer` in place (via
 * strtok_r) and cmd->argv[] ends up pointing *into* `buffer`, so `cmd`
 * is only valid as long as `buffer`'s contents aren't reused.
 *
 * Returns cmd->argc (>= 0) on success, or -1 if the line contains more
 * than MAX_ARGS tokens.
 */
int parse_command(command_t* cmd, char* buffer, ssize_t len);

#endif
