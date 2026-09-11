#ifndef SIGNALS_H
#define SIGNALS_H

#include <signal.h>

extern volatile sig_atomic_t shutdown_requested;

void install_signal_handlers(void);

#endif
