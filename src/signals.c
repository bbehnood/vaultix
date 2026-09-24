#include "signals.h"

#include <errno.h>
#include <signal.h>
#include <string.h>

volatile sig_atomic_t shutdown_requested = 0;
volatile sig_atomic_t last_signal        = 0;

static const int shutdown_signals[] = {SIGINT, SIGTERM, SIGHUP};
#define N_SHUTDOWN_SIGNALS (sizeof(shutdown_signals) / sizeof(shutdown_signals[0]))

static void handle_shutdown_signal(int signum)
{
    const int saved_errno = errno;

    if (shutdown_requested)
    {
        signal(signum, SIG_DFL);
        raise(signum);
    }
    else
    {
        last_signal        = signum;
        shutdown_requested = 1;
    }

    errno = saved_errno;
}

int install_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_shutdown_signal;
    sa.sa_flags   = 0;

    sigemptyset(&sa.sa_mask);
    for (size_t i = 0; i < N_SHUTDOWN_SIGNALS; i++)
    {
        if (sigaddset(&sa.sa_mask, shutdown_signals[i]) < 0)
            return -1;
    }

    for (size_t i = 0; i < N_SHUTDOWN_SIGNALS; i++)
    {
        if (sigaction(shutdown_signals[i], &sa, NULL) < 0)
            return -1;
    }

    struct sigaction ignore;
    memset(&ignore, 0, sizeof(ignore));
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);

    if (sigaction(SIGPIPE, &ignore, NULL) < 0)
        return -1;

    return 0;
}
