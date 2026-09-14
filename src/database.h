#ifndef DATABASE_H
#define DATABASE_H

#include "parser.h"

#include <stddef.h>

#define MAX_KEYS       1000
#define MAX_KEY_SIZE   64
#define MAX_VALUE_SIZE 256

/* One key/value pair. Both fields are fixed-size, NUL-terminated buffers. */
typedef struct entry
{
    char key[MAX_KEY_SIZE];
    char value[MAX_VALUE_SIZE];
} entry_t;

/*
 * The outcome of executing a command, ready to be written to the client
 * by reply.c. `data` is only meaningful for REPLY_OK/ERROR/BULK; for
 * REPLY_NIL and REPLY_UNKNOWN_COMMAND (data field unused by dispatch,
 * except that server.c reads cmd.argv[0] directly for the latter).
 */
typedef struct reply
{
    enum
    {
        REPLY_OK,              /* command succeeded, no payload (e.g. SET) */
        REPLY_ERROR,           /* command failed, data = error message */
        REPLY_BULK,            /* command returned a value, data = value */
        REPLY_NIL,             /* command found nothing (e.g. GET miss) */
        REPLY_UNKNOWN_COMMAND, /* first argument wasn't a known command */
    } type;
    const char* data;
} reply_t;

/*
 * Runs a parsed command (cmd.argc must be > 0) against the in-memory
 * store and returns the reply to send back to the client. Currently
 * supports SET <key> <value> and GET <key>, case-insensitively.
 */
reply_t execute_command(command_t cmd);

#endif
