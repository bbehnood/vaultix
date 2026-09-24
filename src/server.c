#include "server.h"

#include "signals.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE (4096)
#define MAX_EVENTS  (128)

typedef struct client
{
    int            fd;
    char           peer[INET_ADDRSTRLEN + 8];
    struct client* prev;
    struct client* next;
} client_t;

static void handle_client(int client_fd, const char* peer)
{
    (void)client_fd;
    (void)peer;
}

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
    {
        return -1;
    }

    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int epoll_add(int epoll_fd, int fd, uint32_t events, void* ptr)
{
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events   = events;
    ev.data.ptr = ptr;

    return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}

static void client_close(server_t* server, client_t* client)
{
    epoll_ctl(server->epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);

    if (client->prev != NULL)
    {
        client->prev->next = client->next;
    }
    else
    {
        server->clients = client->next;
    }

    if (client->next != NULL)
    {
        client->next->prev = client->prev;
    }

    LOG_INFO("Client %s disconnected", client->peer);

    close(client->fd);
    free(client);
}

static int client_add(server_t* server, int fd, const struct sockaddr_in* addr)
{
    if (set_nonblocking(fd) < 0)
    {
        LOG_PERROR("Failed to set client socket non-blocking");
        return -1;
    }

    client_t* client = calloc(1, sizeof(*client));
    if (client == NULL)
    {
        LOG_PERROR("Failed to allocate client");
        return -1;
    }

    client->fd = fd;
    snprintf(client->peer,
             sizeof(client->peer),
             "%s:%d",
             inet_ntoa(addr->sin_addr),
             ntohs(addr->sin_port));

    if (epoll_add(server->epoll_fd, fd, EPOLLIN | EPOLLRDHUP, client) < 0)
    {
        LOG_PERROR("Failed to register client with epoll");
        free(client);
        return -1;
    }

    client->next = server->clients;
    if (server->clients != NULL)
    {
        server->clients->prev = client;
    }
    server->clients = client;

    LOG_INFO("Client %s connected", client->peer);

    return 0;
}

static void accept_clients(server_t* server)
{
    for (;;)
    {
        struct sockaddr_in client_addr;
        socklen_t          addr_len = sizeof(client_addr);
        int client_fd = accept(server->socket_fd, (struct sockaddr*)&client_addr, &addr_len);

        if (client_fd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return;
            }

            if (errno == EINTR || errno == ECONNABORTED)
            {
                continue;
            }

            LOG_PERROR("Failed to accept connection");
            return;
        }

        if (client_add(server, client_fd, &client_addr) < 0)
        {
            close(client_fd);
        }
    }
}

server_status_t server_init(server_t*     server,
                            int           domain,
                            int           port,
                            int           service,
                            int           protocol,
                            int           backlog,
                            unsigned long interface)
{
    server->domain    = domain;
    server->port      = port;
    server->service   = service;
    server->protocol  = protocol;
    server->backlog   = backlog;
    server->interface = interface;

    server->address.sin_family      = domain;
    server->address.sin_port        = htons(port);
    server->address.sin_addr.s_addr = htonl(interface);

    server->epoll_fd = -1;
    server->clients  = NULL;

    server->socket_fd = socket(domain, service, protocol);
    if (server->socket_fd < 0)
    {
        LOG_PERROR("Failed to create socket");
        return SERVER_ERR_SOCKET;
    }

    int opt = 1;
    if (setsockopt(server->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        LOG_PERROR("Failed to set SO_REUSEADDR (continuing anyway)");
    }

    if (bind(server->socket_fd, (struct sockaddr*)&server->address, sizeof(server->address)) < 0)
    {
        LOG_PERROR("Failed to bind socket");
        close(server->socket_fd);
        return SERVER_ERR_BIND;
    }

    if (listen(server->socket_fd, server->backlog) < 0)
    {
        LOG_PERROR("Failed to start listening");
        close(server->socket_fd);
        return SERVER_ERR_LISTEN;
    }

    if (set_nonblocking(server->socket_fd) < 0)
    {
        LOG_PERROR("Failed to set listening socket non-blocking");
        close(server->socket_fd);
        return SERVER_ERR_SOCKET;
    }

    server->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (server->epoll_fd < 0)
    {
        LOG_PERROR("Failed to create epoll instance");
        close(server->socket_fd);
        return SERVER_ERR_EPOLL;
    }

    if (epoll_add(server->epoll_fd, server->socket_fd, EPOLLIN, NULL) < 0)
    {
        LOG_PERROR("Failed to register listening socket with epoll");
        close(server->epoll_fd);
        close(server->socket_fd);
        return SERVER_ERR_EPOLL;
    }

    LOG_INFO("Listening on %s:%d", inet_ntoa(server->address.sin_addr), port);

    return SERVER_OK;
}

server_status_t server_start(server_t* server)
{
    sigset_t blocked;
    sigset_t original;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGINT);
    sigaddset(&blocked, SIGTERM);
    sigaddset(&blocked, SIGHUP);
    sigprocmask(SIG_BLOCK, &blocked, &original);

    struct epoll_event events[MAX_EVENTS];
    server_status_t    status = SERVER_OK;

    while (!shutdown_requested)
    {
        int n = epoll_pwait(server->epoll_fd, events, MAX_EVENTS, -1, &original);

        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            LOG_PERROR("epoll_wait failed");
            status = SERVER_ERR_EPOLL;
            break;
        }

        for (int i = 0; i < n; i++)
        {
            client_t* client = events[i].data.ptr;

            if (client == NULL)
            {
                accept_clients(server);
                continue;
            }

            uint32_t ev = events[i].events;

            if (ev & EPOLLIN)
            {
                handle_client(client->fd, client->peer);
            }

            if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
            {
                client_close(server, client);
            }
        }
    }

    if (status == SERVER_OK)
    {
        LOG_INFO("Shutting down (signal %d)...", (int)last_signal);
    }

    while (server->clients != NULL)
    {
        client_close(server, server->clients);
    }

    close(server->epoll_fd);
    close(server->socket_fd);

    sigprocmask(SIG_SETMASK, &original, NULL);

    return status;
}
