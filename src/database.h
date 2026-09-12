#ifndef DATABASE_H
#define DATABASE_H

#include "parser.h"

#include <stddef.h>

#define MAX_KEYS       1000
#define MAX_KEY_SIZE   64
#define MAX_VALUE_SIZE 256

typedef struct entry
{
    char key[MAX_KEY_SIZE];
    char value[MAX_VALUE_SIZE];
} entry_t;

typedef struct reply
{
    enum
    {
        REPLY_OK,
        REPLY_ERROR,
        REPLY_BULK,
        REPLY_NIL,
        REPLY_UNKNOWN_COMMAND,
    } type;
    const char* data;
} reply_t;

reply_t execute_command(command_t cmd);

#endif
