#ifndef GLOBALS_H
#define GLOBALS_H

#include <signal.h>  // This defines sig_atomic_t

// Declare the global flag as external
extern volatile sig_atomic_t keepRunning;

// Declare the signal handler prototype
void handle_signal(int sig);

#endif // GLOBALS_H
