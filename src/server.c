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

/*
 * Size of each connection's line-assembly buffer. The protocol is
 * line-based (one command per '\n'-terminated line), so this also caps
 * the maximum command line length; see the overflow check at the bottom
 * of handle_client().
 */
#define BUFFER_SIZE 4096

/*
 * Parses one line into a command and executes it, writing the reply
 * straight to `client_fd`. `line` is a single '\n'-terminated (or
 * buffer-exhausted) chunk carved out of the connection's read buffer by
 * handle_client() - parse_command() is destructive and will tokenize it
 * in place.
 */
static void dispatch(int client_fd, char* line, ssize_t len)
{
    command_t cmd;
    int       argc = parse_command(&cmd, line, len);

    if (argc < 0)
    {
        /* Client-driven, not a server fault - see execute_command() for
         * the same reasoning applied to command-level errors. */
        LOG_DEBUG("Rejected command: too many arguments (max %d)", MAX_ARGS);
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

/*
 * Services one client connection until it disconnects, the server is
 * asked to shut down, or a read error occurs. `peer` is a pre-formatted
 * "ip:port" string used only for log messages.
 *
 * Reads are accumulated into `buffer`; each complete '\n'-terminated
 * line found in it is handed to dispatch() one at a time, and any
 * trailing partial line is shifted to the front of the buffer to be
 * completed by a subsequent read.
 */
static void handle_client(int client_fd, const char* peer)
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

            LOG_PERROR("Failed to read from client %s", peer);
            return;
        }

        if (n == 0)
        {
            LOG_INFO("Client %s disconnected", peer);
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

        /* Shift any incomplete trailing line to the front of the buffer
         * so the next recv() can complete it. */
        size_t leftover = (buffer + buf_len) - line_start;
        memmove(buffer, line_start, leftover);
        buf_len = leftover;

        /* The buffer filled up without ever finding a '\n' - the client
         * is sending a line longer than we support. This is unusual
         * enough (a well-behaved client should never hit it) that it's
         * worth an ERROR rather than DEBUG, so operators notice a
         * misbehaving or hostile client. We drop what we have and keep
         * the connection open rather than disconnecting the client. */
        if (buf_len == sizeof(buffer) - 1)
        {
            LOG_ERROR("Client %s sent a command line longer than %d bytes",
                      peer,
                      BUFFER_SIZE);
            reply_error(client_fd, "ERR command line too long");
            buf_len = 0;
        }
    }
}

/*
 * Creates, configures, binds, and starts listening on the server's
 * socket. `service` is the socket type (e.g. SOCK_STREAM), matching the
 * middle argument of socket(2) - named after the struct field it fills,
 * not to be confused with `protocol`.
 *
 * Returns SERVER_OK on success, or a SERVER_ERR_* code on failure (see
 * server.h); the specific failure is also logged via LOG_PERROR.
 */
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

    /*
     * Non-fatal: without SO_REUSEADDR a quick restart can fail to bind
     * with "Address already in use" while the old socket lingers in
     * TIME_WAIT, but the server can still run without it, so we just
     * log and carry on rather than aborting startup over it.
     */
    int opt = 1;
    if (setsockopt(
            server->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) <
        0)
    {
        LOG_PERROR("Failed to set SO_REUSEADDR (continuing anyway)");
    }

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

/*
 * Accepts and services one client connection at a time (see the note in
 * the code review about this being a single-connection-at-a-time
 * server: a second client cannot connect until the first disconnects).
 * Runs until shutdown_requested is set by a signal handler (see
 * signals.c) or an unrecoverable accept() error occurs.
 */
int server_start(server_t* server)
{
    while (!shutdown_requested)
    {
        struct sockaddr_in client_addr;
        socklen_t          addr_len   = sizeof(client_addr);
        int                client_fd  = accept(server->socket_fd,
                                (struct sockaddr*)&client_addr,
                                &addr_len);

        if (client_fd < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            LOG_PERROR("Failed to accept connection");

            /* These errno values mean the listening socket itself is
             * broken beyond repair (bad/invalid fd) - there's no point
             * retrying accept(), so bail out of the server. Anything
             * else (e.g. a per-connection issue like ECONNABORTED) is
             * transient and worth just retrying. */
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
