#include "signals.h"

#include <signal.h>
#include <string.h>

volatile sig_atomic_t shutdown_requested = 0;
volatile sig_atomic_t last_signal        = 0;

/*
 * Async-signal-safe: only touches sig_atomic_t globals, no I/O, no libc
 * calls that aren't guaranteed safe. Do not add logging here directly -
 * log `last_signal` from server_start() after the accept loop exits.
 */
static void handle_shutdown_signal(int signum)
{
    last_signal        = signum;
    shutdown_requested = 1;
}

void install_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = handle_shutdown_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /*
     * Bug fix: without this, a client that disconnects while we're mid-
     * send() delivers SIGPIPE, whose default action terminates the whole
     * process - taking down every other connection with it. Ignoring it
     * makes send() fail with EPIPE instead, which send_all() in reply.c
     * already handles by simply giving up on that one reply.
     */
    signal(SIGPIPE, SIG_IGN);
}
