#include "h/open_serial_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <libserialport.h>

// Helper function to duplicate string
static char *duplicate_string(const char *str)
{
    char *dup = malloc(strlen(str) + 1);
    if (dup)
        strcpy(dup, str);
    return dup;
}

// Function to remove leading spaces
static char *remove_leading_spaces(char *str)
{
    while (*str && isspace((unsigned char)*str))
        str++; // Skip leading spaces
    return str;
}

// Function to remove unwanted characters
static void remove_unwanted_characters(char *str)
{
    int i = 0, j = 0;
    while (str[i])
    {
        // Allow only letters and a space character
        if (isalpha((unsigned char)str[i]) || (isspace((unsigned char)str[i]) && i > 0 && str[i - 1] != ' '))
        {
            str[j++] = str[i];
        }
        i++;
    }
    str[j] = '\0';
}

// Function to ensure the string starts with a letter
static void ensure_start_with_letter(char *str)
{
    // Remove leading spaces
    char *cleaned = remove_leading_spaces(str);

    // If it doesn't start with a letter, we can change it to "radio"
    if (!isalpha((unsigned char)cleaned[0]))
    {
        strcpy(str, "radio");
        return;
    }

    strcpy(str, cleaned);
}

// Function to remove multiple spaces and only keep one space
static void remove_extra_spaces(char *str)
{
    int i = 0;
    int j = 0;
    int space_found = 0;

    // Process each character
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

// Function to clean the string (only letters and one space)
static void clean_string(char *str)
{
    ensure_start_with_letter(str);
    remove_extra_spaces(str);
    remove_unwanted_characters(str);
}

// Function to remove unwanted endings (AM, AML, L, LL)
static void remove_endings(char *str)
{
    size_t len = strlen(str);

    // Remove "AML"
    if (len >= 3 && strcmp(str + len - 3, "AML") == 0)
        str[len - 3] = '\0';
    // Remove "AM"
    else if (len >= 2 && strcmp(str + len - 2, "AM") == 0)
        str[len - 2] = '\0';
    // Remove "LL"
    else if (len >= 2 && strcmp(str + len - 2, "LL") == 0)
        str[len - 2] = '\0';
    // Remove "L"
    else if (len >= 1 && str[len - 1] == 'L')
        str[len - 1] = '\0';
}

char *open_serial_port(const char *com_port)
{
    struct sp_port *port;
    enum sp_return ret;

    // Get the port by name
    ret = sp_get_port_by_name(com_port, &port);
    if (ret != SP_OK)
    {
        fprintf(stderr, "Error: Could not find port %s\n", com_port);
        return duplicate_string("radio");
    }

    // Open the port for reading
    ret = sp_open(port, SP_MODE_READ);
    if (ret != SP_OK)
    {
        fprintf(stderr, "Error: Could not open port %s\n", com_port);
        sp_free_port(port);
        return duplicate_string("radio");
    }

    // Configure the port: set baud rate to 38400, 8N1
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

    // Allocate a buffer for accumulating result
    char *result = malloc(1024);
    if (!result)
    {
        sp_close(port);
        sp_free_port(port);
        return duplicate_string("radio");
    }
    result[0] = '\0';

    // Buffer for each read
    char buf[256];
    int bytes_read = 0;

    // Loop until we detect marker "II" in our accumulated string.
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

            // Filter out non-printable characters (only ASCII 32 to 126)
            char filtered[256];
            int j = 0;
            for (int i = 0; i < bytes_read; i++)
            {
                if (buf[i] >= 32 && buf[i] <= 126)
                    filtered[j++] = buf[i];
            }
            filtered[j] = '\0';

            // Append filtered data to result
            strncat(result, filtered, 1023 - strlen(result));

            // Look for the marker "II"
            char *marker = strstr(result, "II");
            if (marker != NULL)
            {
                marker += 2; // Skip "II"
                char extracted[1024];
                strncpy(extracted, marker, sizeof(extracted) - 1);
                extracted[sizeof(extracted) - 1] = '\0';

                // Remove unwanted endings (AM, AML, L, LL)
                remove_endings(extracted);

                // Clean up the string
                clean_string(extracted);

                // Clean up and return result
                sp_close(port);
                sp_free_port(port);
                free(result);
                return duplicate_string(extracted);
            }
        }
        // Sleep 100ms to avoid busy-waiting
        usleep(100000);
    }

    sp_close(port);
    sp_free_port(port);
    free(result);
    return duplicate_string("radio");
}