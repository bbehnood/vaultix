#include "reply.h"

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>

/*
 * Writes `len` bytes to `fd`, looping over send() until it's all sent.
 * Gives up silently on error (including EPIPE, which we can now get
 * without crashing the process since SIGPIPE is ignored - see
 * signals.c) - a reply that can't be delivered is dropped, not retried
 * or surfaced to the caller, since there's nothing more this layer can
 * do about a broken connection.
 */
static void send_all(int fd, const char* buf, size_t len)
{
    size_t sent = 0;
    while (sent < len)
    {
        ssize_t n = send(fd, buf + sent, len - sent, 0);
        if (n <= 0)
        {
            return;
        }
        sent += (size_t)n;
    }
}

void reply_simple(int fd, const char* str)
{
    char buf[256];
    int  n = snprintf(buf, sizeof(buf), "+%s\r\n", str);

    /*
     * assert() is the right tool here, not a runtime check: `str` only ever
     * comes from fixed literals we control (see database.c), never from
     * client input, so if this holds under a debug build it holds
     * unconditionally - there's no way for it to start failing only in a
     * release (NDEBUG) build.
     */
    assert(n >= 0 && (size_t)n < sizeof(buf));

    send_all(fd, buf, (size_t)n);
}

void reply_error(int fd, const char* fmt, ...)
{
    char    msg[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    char buf[300];
    int  n = snprintf(buf, sizeof(buf), "-%s\r\n", msg);

    send_all(fd, buf, (size_t)n);
}

void reply_integer(int fd, long long n)
{
    char buf[32];
    int  len = snprintf(buf, sizeof(buf), "%lld\r\n", n);

    send_all(fd, buf, (size_t)len);
}

void reply_bulk(int fd, const char* str, size_t len)
{
    if (!str)
    {
        /* "$-1\r\n" is the RESP nil bulk string, used for a GET miss. */
        send_all(fd, "$-1\r\n", 5);
        return;
    }

    char header[32];
    int  hlen = snprintf(header, sizeof(header), "$%zu\r\n", len);

    send_all(fd, header, (size_t)hlen);
    send_all(fd, str, (size_t)len);
    send_all(fd, "\r\n", 2);
}
