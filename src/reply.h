#ifndef REPLY_H
#define REPLY_H

#include <stddef.h>

void reply_simple(int fd, const char* str);
void reply_error(int fd, const char* fmt, ...);
void reply_integer(int fd, long long n);
void reply_bulk(int fd, const char* str, size_t len);

#endif
