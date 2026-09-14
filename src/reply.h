#ifndef REPLY_H
#define REPLY_H

#include <stddef.h>

/*
 * Wire-format helpers for replying to a client, loosely modeled on
 * Redis's RESP protocol. Each function writes its reply synchronously
 * and best-effort: if the write fails partway (e.g. the client already
 * disconnected), it simply stops rather than retrying or reporting an
 * error to the caller - see send_all() in reply.c.
 */

/* "+<str>\r\n" - a short, trusted status string (e.g. "OK"). */
void reply_simple(int fd, const char* str);

/* "-<formatted message>\r\n" - an error; fmt/... work like printf. */
void reply_error(int fd, const char* fmt, ...);

/* "<n>\r\n" - a bare integer reply (currently unused by database.c). */
void reply_integer(int fd, long long n);

/*
 * "$<len>\r\n<str>\r\n", or "$-1\r\n" if `str` is NULL (a "nil" reply,
 * e.g. for a GET on a missing key). `len` is the byte length of `str`
 * and is not expected to include a NUL terminator.
 */
void reply_bulk(int fd, const char* str, size_t len);

#endif
