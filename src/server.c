#include "server.h"

#include "signals.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

int server_init(server_t*     server,
                int           domain,
                int           port,
                int           service,
                int           protocol,
                int           backlog,
                unsigned long interface)

{
    server->domain    = domain;
    server->service   = service;
    server->port      = port;
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
    setsockopt(server->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server->socket_fd,
             (struct sockaddr*)&server->address,
             sizeof(server->address)) < 0)
    {
        LOG_PERROR("Failed to bind socket");
        return SERVER_ERR_BIND;
    }

    if (listen(server->socket_fd, server->backlog) < 0)
    {
        LOG_PERROR("Failed to start listening");
        return SERVER_ERR_LISTEN;
    }

    LOG_INFO("Listening on %s:%d", inet_ntoa(server->address.sin_addr), port);

    return SERVER_OK;
}

int server_start(server_t* server)
{
    while (!shutdown_requested)
    {
        int client_fd = accept(server->socket_fd, NULL, NULL);

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

        char buffer[BUFFER_SIZE];

        LOG_INFO("Client connected");

        ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (n < 0)
        {
            LOG_PERROR("Failed to read from client");
        }
        else if (n == 0)
        {
            LOG_INFO("Client disconnected before sending data");
        }
        else
        {
            buffer[n] = '\0';
            LOG_INFO("Received: %s", buffer);
        }

        close(client_fd);
    }

    LOG_INFO("Shutting down...");
    close(server->socket_fd);

    return 0;
}
