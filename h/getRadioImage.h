#ifndef GETRADIOIMAGE_H
#define GETRADIOIMAGE_H

#include <stdint.h>

int radio_init(const char *port);

void radio_update(void);

void radio_close(void);

#endif // GETRADIOIMAGE_H