#ifndef SERVER_H
#define SERVER_H

#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>

/*
 * Logging macros used throughout the codebase:
 *   LOG_INFO   - normal operational events (startup, connect/disconnect).
 *   LOG_ERROR  - unexpected conditions worth an operator's attention
 *                (failed syscalls, resource exhaustion); use LOG_PERROR
 *                instead when errno is relevant.
 *   LOG_PERROR - like LOG_ERROR, but also appends strerror(errno).
 *   LOG_DEBUG  - compiled out unless built with -DDEBUG; used for
 *                per-request tracing and client-caused errors (bad
 *                arguments, unknown commands) that are expected under
 *                malformed input and shouldn't spam production logs.
 */
#define LOG_INFO(...)                                                          \
    do                                                                         \
    {                                                                          \
        fprintf(stdout, "[INFO] " __VA_ARGS__);                                \
        fprintf(stdout, "\n");                                                 \
    } while (0)

#define LOG_ERROR(...)                                                         \
    do                                                                         \
    {                                                                          \
        fprintf(stderr, "[ERROR] " __VA_ARGS__);                               \
        fprintf(stderr, "\n");                                                 \
    } while (0)

#define LOG_PERROR(...)                                                        \
    do                                                                         \
    {                                                                          \
        fprintf(stderr, "[ERROR] " __VA_ARGS__);                               \
        fprintf(stderr, ": %s\n", strerror(errno));                            \
    } while (0)

#ifdef DEBUG
#define LOG_DEBUG(...)                                                         \
    do                                                                         \
    {                                                                          \
        fprintf(stdout, "[DEBUG] %s:%d: ", __func__, __LINE__);                \
        fprintf(stdout, __VA_ARGS__);                                          \
        fprintf(stdout, "\n");                                                 \
    } while (0)
#else
#define LOG_DEBUG(...)                                                         \
    do                                                                         \
    {                                                                          \
    } while (0)
#endif

/* Configuration and state for the listening TCP socket. */
typedef struct server
{
    int           domain;    /* address family, e.g. AF_INET */
    int           port;      /* port to listen on, host byte order */
    int           service;   /* socket type, e.g. SOCK_STREAM */
    int           protocol;  /* e.g. 0 to let the kernel pick */
    int           backlog;   /* pending-connection queue size for listen() */
    unsigned long interface; /* bind address, host byte order (e.g. INADDR_ANY) */

    int                socket_fd;
    struct sockaddr_in address;

} server_t;

/* Return codes for server_init()/server_start(); SERVER_OK is always 0. */
typedef enum server_status
{
    SERVER_OK         = 0,
    SERVER_ERR_SOCKET = -1, /* socket() failed */
    SERVER_ERR_BIND   = -2, /* bind() failed */
    SERVER_ERR_LISTEN = -3, /* listen() failed */
    SERVER_ERR_ACCEPT = -4, /* accept() failed unrecoverably; see server.c */
} server_status_t;

int server_init(server_t*     server,
                int           domain,
                int           port,
                int           service,
                int           protocol,
                int           backlog,
                unsigned long interface);

/* Runs the accept loop until shutdown_requested is set. See server.c. */
int server_start(server_t* server);

#endif
