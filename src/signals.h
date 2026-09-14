#ifndef SIGNALS_H
#define SIGNALS_H

#include <signal.h>

/*
 * Set to 1 by our SIGINT/SIGTERM handler. The accept()/recv() loops in
 * server.c poll this flag so the server can shut down cleanly instead of
 * dying mid-request.
 */
extern volatile sig_atomic_t shutdown_requested;

/*
 * The signal number that triggered the shutdown (0 if none yet). Only
 * used for logging after the main loop has exited, never inside the
 * handler itself, since fprintf() is not async-signal-safe.
 */
extern volatile sig_atomic_t last_signal;

/*
 * Installs handlers for SIGINT/SIGTERM (graceful shutdown) and ignores
 * SIGPIPE so that writing to a socket a client has already closed
 * returns EPIPE from send() instead of killing the whole process.
 */
void install_signal_handlers(void);

#endif
