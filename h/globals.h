#ifndef GLOBALS_H
#define GLOBALS_H

#include <signal.h> 

extern volatile sig_atomic_t keepRunning;

void handle_signal(int sig);

#endif
