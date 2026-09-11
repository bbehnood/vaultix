#include "signals.h"

#include <signal.h>
#include <string.h>

volatile sig_atomic_t shutdown_requested = 0;

static void handle_shutdown_signal(int signum)
{
    (void)signum;
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
}
