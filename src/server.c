#include "server.h"

#include "signals.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE (4096)

static void handle_client(int client_fd, const char* peer)
{
    (void)client_fd;
    (void)peer;
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

    LOG_INFO("Listening on %s:%d", inet_ntoa(server->address.sin_addr), port);

    return SERVER_OK;
}

server_status_t server_start(server_t* server)
{
    while (!shutdown_requested)
    {
        struct sockaddr_in client_addr;
        socklen_t          addr_len = sizeof(client_addr);
        int client_fd = accept(server->socket_fd, (struct sockaddr*)&client_addr, &addr_len);

        if (client_fd < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            LOG_PERROR("Failed to accept connection");

            if (errno == EBADF || errno == EINVAL || errno == ENOTSOCK)
            {
                close(server->socket_fd);
                return SERVER_ERR_ACCEPT;
            }

            continue;
        }

        char peer[INET_ADDRSTRLEN + 8];
        snprintf(peer,
                 sizeof(peer),
                 "%s:%d",
                 inet_ntoa(client_addr.sin_addr),
                 ntohs(client_addr.sin_port));

        LOG_INFO("Client %s connected", peer);

        handle_client(client_fd, peer);

        close(client_fd);
    }

    LOG_INFO("Shutting down (signal %d)...", (int)last_signal);
    close(server->socket_fd);

    return 0;
}
