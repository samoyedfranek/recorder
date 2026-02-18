#include "h/open_serial_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libserialport.h>
#include <pthread.h>

static char name_history[3][128] = {"radio", "radio", "radio"};
static pthread_mutex_t radio_name_mutex = PTHREAD_MUTEX_INITIALIZER;

char *get_radio_name(void)
{
    pthread_mutex_lock(&radio_name_mutex);
    char *dup = malloc(strlen(name_history[0]) + 1);
    if (dup)
        strcpy(dup, name_history[0]);
    pthread_mutex_unlock(&radio_name_mutex);
    return dup;
}

void *serial_monitor_thread(void *arg)
{
    const char *com_port = (const char *)arg;
    struct sp_port *port;

    printf("[Serial] Monitor thread starting. Looking for %s...\n", com_port);

    while (1)
    {
        if (sp_get_port_by_name(com_port, &port) == SP_OK)
        {
            if (sp_open(port, SP_MODE_READ) == SP_OK)
            {
                sp_set_baudrate(port, 38400);
                sp_set_bits(port, 8);
                sp_set_parity(port, SP_PARITY_NONE);
                sp_set_stopbits(port, 1);
                sp_flush(port, SP_BUF_INPUT);

                char buf[1];
                char word_buffer[128] = {0};
                int idx = 0;

                while (1)
                {
                    int bytes = sp_blocking_read(port, buf, 1, 20);

                    if (bytes > 0)
                    {
                        if (buf[0] == '\n' || buf[0] == '\r')
                        {
                            if (idx > 0)
                            {
                                word_buffer[idx] = '\0';

                                pthread_mutex_lock(&radio_name_mutex);
                                strncpy(name_history[2], name_history[1], 128);
                                strncpy(name_history[1], name_history[0], 128);
                                strncpy(name_history[0], word_buffer, 128);
                                pthread_mutex_unlock(&radio_name_mutex);
                                idx = 0;
                            }
                        }
                        else if (buf[0] >= 32 && buf[0] <= 126)
                        {
                            if (idx < sizeof(word_buffer) - 1)
                                word_buffer[idx++] = buf[0];
                        }
                    }
                    else if (bytes < 0)
                    {
                        printf("\n[Serial] ERROR: Connection lost.\n");
                        break;
                    }
                    else
                    {
                        if (idx > 0)
                        {
                            word_buffer[idx] = '\0';

                            pthread_mutex_lock(&radio_name_mutex);
                            strncpy(name_history[2], name_history[1], 128);
                            strncpy(name_history[1], name_history[0], 128);
                            strncpy(name_history[0], word_buffer, 128);
                            pthread_mutex_unlock(&radio_name_mutex);
                            idx = 0;
                        }
                    }
                }
                sp_close(port);
            }
            sp_free_port(port);
        }
        sleep(1);
    }
    return NULL;
}