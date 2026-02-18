#include "h/open_serial_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libserialport.h>

// Helper to duplicate the string so it can be safely returned and freed later
static char *duplicate_string(const char *str)
{
    char *dup = malloc(strlen(str) + 1);
    if (dup)
        strcpy(dup, str);
    return dup;
}

char *open_serial_port(const char *com_port)
{
    struct sp_port *port;
    enum sp_return ret;

    // 1. Locate the port
    ret = sp_get_port_by_name(com_port, &port);
    if (ret != SP_OK)
    {
        fprintf(stderr, "Error: Could not find port %s\n", com_port);
        return duplicate_string("radio");
    }

    // 2. Open the port
    ret = sp_open(port, SP_MODE_READ);
    if (ret != SP_OK)
    {
        fprintf(stderr, "Error: Could not open port %s\n", com_port);
        sp_free_port(port);
        return duplicate_string("radio");
    }

    // 3. Configure port settings (38400 Baud, 8-N-1)
    ret = sp_set_baudrate(port, 38400);
    if (ret != SP_OK)
    {
        fprintf(stderr, "Error: Could not set baud rate\n");
        sp_close(port);
        sp_free_port(port);
        return duplicate_string("radio");
    }
    
    ret = sp_set_bits(port, 8);
    ret |= sp_set_parity(port, SP_PARITY_NONE);
    ret |= sp_set_stopbits(port, 1);
    ret |= sp_set_flowcontrol(port, SP_FLOWCONTROL_NONE);
    
    if (ret != SP_OK)
    {
        fprintf(stderr, "Error: Could not configure port parameters\n");
        sp_close(port);
        sp_free_port(port);
        return duplicate_string("radio");
    }

    // Buffer to hold incoming data
    char result[1024] = {0};
    char buf[256];
    int bytes_read = 0;

    // 4. Read Loop
    while (1)
    {
        ret = sp_nonblocking_read(port, buf, sizeof(buf) - 1);
        
        if (ret < 0)
        {
            fprintf(stderr, "Error reading from port\n");
            break;
        }
        else if (ret > 0)
        {
            bytes_read = ret;
            buf[bytes_read] = '\0';

            // Append new characters to our result buffer
            strncat(result, buf, sizeof(result) - strlen(result) - 1);

            // Check if the radio finished sending the name (signaled by '\n')
            char *newline = strchr(result, '\n');
            if (newline != NULL)
            {
                *newline = '\0'; // Terminate the string at the newline

                // Strip the Carriage Return ('\r') if it's there
                int len = strlen(result);
                if (len > 0 && result[len - 1] == '\r')
                {
                    result[len - 1] = '\0';
                    len--;
                }

                // If we got a valid, non-empty name, close port and return it
                if (len > 0)
                {
                    sp_close(port);
                    sp_free_port(port);
                    return duplicate_string(result);
                }
                else
                {
                    // If it was just an empty line, clear the buffer and keep waiting
                    result[0] = '\0';
                }
            }
        }

        // Wait 100ms before checking the serial port again to save CPU usage
        usleep(100000); 
    }

    // Fallback if the loop breaks
    sp_close(port);
    sp_free_port(port);
    return duplicate_string("radio");
}