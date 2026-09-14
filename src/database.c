#include "database.h"

#include "server.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

/*
 * The whole "database" is a flat array scanned linearly by set()/get().
 * That's O(n) per operation, which is fine at MAX_KEYS = 1000 but should
 * be swapped for a hash table if that limit ever grows.
 *
 * Not thread-safe: this is fine today because server.c only ever
 * services one client connection at a time, but if the server ever
 * gains concurrency (threads/fork/epoll), these globals will need a
 * lock (or a redesign) before that happens.
 */
static entry_t db[MAX_KEYS];
static size_t  db_size = 0;

typedef enum set_status
{
    SET_OK,
    SET_ERR_KEY_TOO_LONG,
    SET_ERR_VALUE_TOO_LONG,
    SET_ERR_DB_FULL,
} set_status_t;

/*
 * Insert `key` = `value`, or overwrite the value if `key` already exists.
 * Both strings are copied into the entry; the caller's buffers (which
 * point into the connection's line buffer) can be reused/overwritten
 * afterwards.
 */
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
            /*
             * Bug fix: this used to re-copy `key` over db[i].key, which
             * left db[i].value untouched - i.e. SET on an existing key
             * was a silent no-op instead of updating the value. We want
             * to overwrite the *value* here; the key is already correct
             * since we just matched it above.
             */
            memcpy(db[i].value, value, value_len + 1);
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

/* Returns a pointer to the stored value for `key`, or NULL if not found. */
static char* get(char* key)
{
    for (size_t i = 0; i < db_size; i++)
    {
        if (strcmp(db[i].key, key) == 0) return db[i].value;
    }

    return NULL;
}

/*
 * Dispatches a parsed command to its handler and builds the reply.
 *
 * Logging levels here follow one rule: anything caused by the *client*
 * sending something we don't like (wrong arity, oversized key/value,
 * unknown command) is LOG_DEBUG - it's expected traffic under malformed
 * or hostile input and shouldn't spam production logs, but it's still
 * useful when debugging a misbehaving client. A full database, on the
 * other hand, is a server-side resource condition an operator would
 * want to know about, so that one stays at LOG_ERROR.
 */
reply_t execute_command(command_t cmd)
{
    reply_t r;

    assert(cmd.argc > 0);

    if (strcasecmp(cmd.argv[0], "SET") == 0)
    {
        if (cmd.argc != 3)
        {
            LOG_DEBUG("Rejected SET: expected 2 arguments, got %d",
                      cmd.argc - 1);

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
                LOG_DEBUG("Rejected SET: key '%s' exceeds %d bytes",
                          cmd.argv[1],
                          MAX_KEY_SIZE - 1);

                r.type = REPLY_ERROR;
                r.data = "ERR key too long";
                break;

            case SET_ERR_VALUE_TOO_LONG:
                LOG_DEBUG("Rejected SET: value for key '%s' exceeds %d bytes",
                          cmd.argv[1],
                          MAX_VALUE_SIZE - 1);

                r.type = REPLY_ERROR;
                r.data = "ERR value too long";
                break;

            case SET_ERR_DB_FULL:
                LOG_ERROR(
                    "Rejected SET '%s': database is full (%d/%d keys)",
                    cmd.argv[1],
                    (int)db_size,
                    MAX_KEYS);

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
            LOG_DEBUG("Rejected GET: expected 1 argument, got %d",
                      cmd.argc - 1);

            r.type = REPLY_ERROR;
            r.data = "ERR wrong number of arguments for 'get' command";
        }
        else
        {
            r.data = get(cmd.argv[1]);
            r.type = (r.data) ? REPLY_BULK : REPLY_NIL;

            LOG_DEBUG("GET %s -> %s",
                      cmd.argv[1],
                      r.data ? r.data : "(nil)");
        }
    }
    else
    {
        LOG_DEBUG("Rejected unknown command '%s'", cmd.argv[0]);

        r.type = REPLY_UNKNOWN_COMMAND;
        r.data = NULL;
    }

    return r;
}
