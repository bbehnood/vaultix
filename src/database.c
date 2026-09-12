#include "database.h"

#include "server.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

entry_t db[MAX_KEYS];
size_t  db_size = 0;

typedef enum set_status
{
    SET_OK,
    SET_ERR_KEY_TOO_LONG,
    SET_ERR_VALUE_TOO_LONG,
    SET_ERR_DB_FULL,
} set_status_t;

static set_status_t set(char* key, char* value)
{
    size_t key_len   = strlen(key);
    size_t value_len = strlen(value);

    if (key_len >= MAX_KEY_SIZE)
    {
        return SET_ERR_KEY_TOO_LONG;
    }

    if (value_len >= MAX_VALUE_SIZE)
    {
        return SET_ERR_VALUE_TOO_LONG;
    }

    for (size_t i = 0; i < db_size; i++)
    {
        if (strcmp(db[i].key, key) == 0)
        {
            memcpy(db[i].key, key, key_len + 1);
            return SET_OK;
        }
    }

    if (db_size >= MAX_KEYS)
    {
        return SET_ERR_DB_FULL;
    }

    memcpy(db[db_size].key, key, key_len + 1);
    memcpy(db[db_size].value, value, value_len + 1);
    db_size++;

    return SET_OK;
}

static char* get(char* key)
{
    for (size_t i = 0; i < db_size; i++)
    {
        if (strcmp(db[i].key, key) == 0) return db[i].value;
    }

    return NULL;
}

reply_t execute_command(command_t cmd)
{
    reply_t r;

    assert(cmd.argc > 0);

    if (strcasecmp(cmd.argv[0], "SET") == 0)
    {
        if (cmd.argc != 3)
        {
            LOG_ERROR("Wrong number of arguments for 'SET' command");

            r.type = REPLY_ERROR;
            r.data = "ERR wrong number of arguments for 'set' command";
        }
        else
        {
            LOG_DEBUG("SET %s = %s", cmd.argv[1], cmd.argv[2]);

            switch (set(cmd.argv[1], cmd.argv[2]))
            {
            case SET_OK:
                r.type = REPLY_OK;
                r.data = "OK";
                break;

            case SET_ERR_KEY_TOO_LONG:
                r.type = REPLY_ERROR;
                r.data = "ERR key too long";
                break;

            case SET_ERR_VALUE_TOO_LONG:
                r.type = REPLY_ERROR;
                r.data = "ERR value too long";
                break;

            case SET_ERR_DB_FULL:
                r.type = REPLY_ERROR;
                r.data = "ERR database is full";
                break;
            }
        }
    }
    else if (strcasecmp(cmd.argv[0], "GET") == 0)
    {
        if (cmd.argc != 2)
        {
            LOG_ERROR("Wrong number of arguments for 'GET' commmand");

            r.type = REPLY_ERROR;
            r.data = "ERR wrong number of arguments for 'get' command";
        }
        else
        {
            LOG_DEBUG("GET %s", cmd.argv[1]);

            r.data = get(cmd.argv[1]);
            r.type = (r.data) ? REPLY_BULK : REPLY_NIL;
        }
    }
    else
    {
        r.type = REPLY_UNKNOWN_COMMAND;
        r.data = NULL;
    }

    return r;
}
