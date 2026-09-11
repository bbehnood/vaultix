#include "server.h"
#include "signals.h"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#define DEFAULT_PORT 6379
#define MIN_PORT     1
#define MAX_PORT     65535

static int parse_port(const char* str, int* out_port)
{
    char* endptr;
    errno = 0;

    long val = strtol(str, &endptr, 10);

    if (errno == ERANGE || val < MIN_PORT || val > MAX_PORT)
    {
        fprintf(stderr, "Port must be between %d and %d\n", MIN_PORT, MAX_PORT);
        return -1;
    }

    if (endptr == str || *endptr != '\0')
    {
        fprintf(stderr, "Invalid port: '%s'\n", str);
        return -1;
    }

    *out_port = (int)val;

    return 0;
}

int main(int argc, char* argv[])
{
    int port = DEFAULT_PORT;

    if (argc > 2)
    {
        fprintf(stderr, "Usage: vaultix [port]\n");
        return EXIT_FAILURE;
    }

    if (argc == 2)
    {
        if (parse_port(argv[1], &port) < 0)
        {
            return EXIT_FAILURE;
        }
    }

    install_signal_handlers();

    server_t server;
    if (server_init(&server, AF_INET, port, SOCK_STREAM, 0, 10, INADDR_ANY) < 0)
    {
        return EXIT_FAILURE;
    }

    int rc = server_start(&server);

    return (rc < 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
