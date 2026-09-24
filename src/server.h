#ifndef SERVER_H
#define SERVER_H

#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>

#define LOG_INFO(...)                                                                              \
    do                                                                                             \
    {                                                                                              \
        fprintf(stdout, "[INFO] " __VA_ARGS__);                                                    \
        fprintf(stdout, "\n");                                                                     \
    } while (0)

#define LOG_ERROR(...)                                                                             \
    do                                                                                             \
    {                                                                                              \
        fprintf(stderr, "[ERROR] " __VA_ARGS__);                                                   \
        fprintf(stderr, "\n");                                                                     \
    } while (0)

#define LOG_PERROR(...)                                                                            \
    do                                                                                             \
    {                                                                                              \
        fprintf(stderr, "[ERROR] " __VA_ARGS__);                                                   \
        fprintf(stderr, ": %s\n", strerror(errno));                                                \
    } while (0)

#ifdef DEBUG
#define LOG_DEBUG(...)                                                                             \
    do                                                                                             \
    {                                                                                              \
        fprintf(stdout, "[DEBUG] %s:%d: ", __func__, __LINE__);                                    \
        fprintf(stdout, __VA_ARGS__);                                                              \
        fprintf(stdout, "\n");                                                                     \
    } while (0)
#else
#define LOG_DEBUG(...)                                                                             \
    do                                                                                             \
    {                                                                                              \
    } while (0)
#endif

struct client;

typedef struct server
{
    int           domain;
    int           port;
    int           service;
    int           protocol;
    int           backlog;
    unsigned long interface;

    int                socket_fd;
    int                epoll_fd;
    struct sockaddr_in address;
    struct client*     clients;
} server_t;

typedef enum server_status
{
    SERVER_OK = 0,
    SERVER_ERR_SOCKET,
    SERVER_ERR_BIND,
    SERVER_ERR_LISTEN,
    SERVER_ERR_ACCEPT,
    SERVER_ERR_EPOLL,
} server_status_t;

server_status_t server_init(server_t*     server,
                            int           domain,
                            int           port,
                            int           service,
                            int           protocol,
                            int           backlog,
                            unsigned long interface);

server_status_t server_start(server_t* server);

#endif
