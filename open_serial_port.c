#include "h/open_serial_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <libserialport.h>

static char *duplicate_string(const char *str)
{
    char *dup = malloc(strlen(str) + 1);
    if (dup)
        strcpy(dup, str);
    return dup;
}

static char *remove_leading_spaces(char *str)
{
    while (*str && isspace((unsigned char)*str))
        str++;
    return str;
}

static void remove_unwanted_characters(char *str)
{
    int i = 0, j = 0;
    while (str[i])
    {

        if (isalpha((unsigned char)str[i]) || (isspace((unsigned char)str[i]) && i > 0 && str[i - 1] != ' '))
        {
            str[j++] = str[i];
        }
        i++;
    }
    str[j] = '\0';
}

static void ensure_start_with_letter(char *str)
{

    char *cleaned = remove_leading_spaces(str);

    if (!isalpha((unsigned char)cleaned[0]))
    {
        strcpy(str, "radio");
        return;
    }

    strcpy(str, cleaned);
}

static void remove_extra_spaces(char *str)
{
    int i = 0;
    int j = 0;
    int space_found = 0;

    while (str[i])
    {
        if (isspace((unsigned char)str[i]))
        {
            if (space_found == 0 && i != 0)
            {
                str[j++] = ' ';
                space_found = 1;
            }
        }
        else
        {
            str[j++] = str[i];
            space_found = 0;
        }
        i++;
    }
    str[j] = '\0';
}

static void clean_string(char *str)
{
    ensure_start_with_letter(str);
    remove_extra_spaces(str);
    remove_unwanted_characters(str);
}

static void remove_endings(char *str)
{
    const char *endings[] = {"AML", "AM", "LL", "LM", "L"};
    size_t len = strlen(str);

    for (int i = 0; i < sizeof(endings) / sizeof(endings[0]); i++)
    {
        char *pos = strstr(str, endings[i]);
        if (pos != NULL)
        {

            *pos = '\0';
            break;
        }
    }
}

char *open_serial_port(const char *com_port)
{
    struct sp_port *port;
    enum sp_return ret;

    ret = sp_get_port_by_name(com_port, &port);
    if (ret != SP_OK)
    {
        fprintf(stderr, "Error: Could not find port %s\n", com_port);
        return duplicate_string("radio");
    }

    ret = sp_open(port, SP_MODE_READ);
    if (ret != SP_OK)
    {
        fprintf(stderr, "Error: Could not open port %s\n", com_port);
        sp_free_port(port);
        return duplicate_string("radio");
    }

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

    char *result = malloc(1024);
    if (!result)
    {
        sp_close(port);
        sp_free_port(port);
        return duplicate_string("radio");
    }
    result[0] = '\0';

    char buf[256];
    int bytes_read = 0;

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

            char filtered[256];
            int j = 0;
            for (int i = 0; i < bytes_read; i++)
            {
                if (buf[i] >= 32 && buf[i] <= 126)
                    filtered[j++] = buf[i];
            }
            filtered[j] = '\0';

            strncat(result, filtered, 1023 - strlen(result));

            char *marker = strstr(result, "II");
            if (marker != NULL)
            {
                marker += 2;
                char extracted[1024];
                strncpy(extracted, marker, sizeof(extracted) - 1);
                extracted[sizeof(extracted) - 1] = '\0';

                remove_endings(extracted);

                clean_string(extracted);

                sp_close(port);
                sp_free_port(port);
                free(result);
                return duplicate_string(extracted);
            }
        }

        usleep(100000);
    }

    sp_close(port);
    sp_free_port(port);
    free(result);
    return duplicate_string("radio");
}