#include "server.h"

#include "database.h"
#include "parser.h"
#include "reply.h"
#include "signals.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

static void dispatch(int client_fd, char* line, ssize_t len)
{
    command_t cmd;
    int       argc = parse_command(&cmd, line, len);

    if (argc < 0)
    {
        LOG_ERROR("Too many arguments");
        reply_error(client_fd, "ERR too many arguments");
    }
    else if (argc == 0)
    {
        /* blank line, nothing to do */
    }
    else
    {
        reply_t r = execute_command(cmd);

        switch (r.type)
        {
        case REPLY_OK:
            reply_simple(client_fd, r.data);
            break;

        case REPLY_ERROR:
            reply_error(client_fd, r.data);
            break;

        case REPLY_BULK:
            reply_bulk(client_fd, r.data, strlen(r.data));
            break;

        case REPLY_NIL:
            reply_bulk(client_fd, NULL, 0);
            break;

        case REPLY_UNKNOWN_COMMAND:
            reply_error(client_fd, "ERR unknown command '%s'", cmd.argv[0]);
            break;
        }
    }
}

static void handle_client(int client_fd)
{
    char   buffer[BUFFER_SIZE];
    size_t buf_len = 0;

    while (!shutdown_requested)
    {
        ssize_t n =
            recv(client_fd, buffer + buf_len, sizeof(buffer) - 1 - buf_len, 0);

        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            LOG_PERROR("Failed to read from client");
            return;
        }

        if (n == 0)
        {
            LOG_INFO("Client disconnected");
            return;
        }

        buf_len += (size_t)n;
        buffer[buf_len] = '\0';

        char* line_start = buffer;

        for (;;)
        {
            char* newline =
                memchr(line_start, '\n', (buffer + buf_len) - line_start);

            if (!newline)
            {
                break;
            }

            ssize_t line_len = newline - line_start + 1;
            dispatch(client_fd, line_start, line_len);

            line_start = newline + 1;
        }

        size_t leftover = (buffer + buf_len) - line_start;
        memmove(buffer, line_start, leftover);
        buf_len = leftover;

        if (buf_len == sizeof(buffer) - 1)
        {
            LOG_ERROR("Command line too long");
            reply_error(client_fd, "ERR command line too long");
            buf_len = 0;
        }
    }
}

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

        LOG_INFO("Client connected");

        handle_client(client_fd);

        close(client_fd);
    }

    LOG_INFO("Shutting down...");
    close(server->socket_fd);

    return 0;
}
