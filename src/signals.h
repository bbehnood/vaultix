#ifndef SIGNALS_H
#define SIGNALS_H

#include <signal.h>

extern volatile sig_atomic_t shutdown_requested;

extern volatile sig_atomic_t last_signal;

int install_signal_handlers(void);

#endif
