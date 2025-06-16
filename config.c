#include "h/config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <portaudio.h>
#include <portaudio.h>

int AUDIO_INPUT_DEVICE_ID = -1;
int AUDIO_OUTPUT_DEVICE_ID = -1;

// Config variables with default/empty values
char BOT_TOKEN[256] = "";
char CHAT_ID[256] = "";
char *CHAT_IDS[21] = {NULL}; // max 20 IDs + NULL terminator
int chat_ids_count = 0;
char COM_PORT[128] = "";
char RECORDING_DIRECTORY[128] = "";
char AUDIO_INPUT_DEVICE[64] = "";
char AUDIO_OUTPUT_DEVICE[64] = "";
char USER_NAME[64] = "";
char WORKDIR[128] = "";
char RECORDER_CMD[256] = "";
char REPO_BRANCH[64] = "";
int AMPLITUDE_THRESHOLD = 0;
int CHUNK_SIZE = 0;
bool DEBUG_AMPLITUDE = false;
bool LIVE_LISTEN = false;
char EXTRA_TEXT[64] = "";
int SILENCE_THRESHOLD = 0;
int REMOVE_LAST_SECONDS = 0;
int get_device_index_by_name(const char *device_name, int is_input)
{
    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0)
    {
        fprintf(stderr, "ERROR: Pa_GetDeviceCount returned %d\n", numDevices);
        return -1;
    }
    const PaDeviceInfo *deviceInfo;

    for (int i = 0; i < numDevices; i++)
    {
        deviceInfo = Pa_GetDeviceInfo(i);
        if (!deviceInfo)
            continue;
        const char *name = deviceInfo->name;
        int hasInput = deviceInfo->maxInputChannels > 0;
        int hasOutput = deviceInfo->maxOutputChannels > 0;
        if (name && strstr(name, device_name) != NULL)
        {
            if (is_input && hasInput)
            {
                return i;
            }
            else if (!is_input && hasOutput)
            {
                return i;
            }
        }
    }

    printf("No suitable %s device found matching name \"%s\"\n",
           is_input ? "input" : "output", device_name);
    return -1; // Not found
}

// Free previously allocated CHAT_IDS strings to prevent memory leaks
void free_chat_ids()
{
    for (int i = 0; i < chat_ids_count; i++)
    {
        free(CHAT_IDS[i]);
        CHAT_IDS[i] = NULL;
    }
    chat_ids_count = 0;
    CHAT_IDS[0] = NULL;
}

// Parse CHAT_ID string (comma-separated) into CHAT_IDS array
void parse_chat_id_array(const char *chat_id_str)
{
    free_chat_ids();

    if (!chat_id_str || strlen(chat_id_str) == 0)
    {
        return;
    }

    char temp[512];
    strncpy(temp, chat_id_str, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *token = strtok(temp, ",");
    chat_ids_count = 0;

    while (token != NULL && chat_ids_count < 20)
    {
        // Trim leading spaces
        while (*token == ' ')
            token++;

        // Trim trailing spaces
        char *end = token + strlen(token) - 1;
        while (end > token && isspace((unsigned char)*end))
        {
            *end = '\0';
            end--;
        }

        CHAT_IDS[chat_ids_count] = strdup(token);
        if (!CHAT_IDS[chat_ids_count])
        {
            fprintf(stderr, "Memory allocation failed for CHAT_IDS\n");
            break;
        }

        chat_ids_count++;
        token = strtok(NULL, ",");
    }

    CHAT_IDS[chat_ids_count] = NULL; // Null-terminate the array
}

// Helper to trim leading and trailing whitespace in-place
static void trim(char *str)
{
    if (!str)
        return;

    // Trim leading whitespace
    char *start = str;
    while (isspace((unsigned char)*start))
        start++;

    if (start != str)
        memmove(str, start, strlen(start) + 1);

    // Trim trailing whitespace
    char *end = str + strlen(str) - 1;
    while (end >= str && isspace((unsigned char)*end))
    {
        *end = '\0';
        end--;
    }
}

// Parse boolean from string
static bool parse_bool(const char *str)
{
    return (strcmp(str, "1") == 0 || strcasecmp(str, "true") == 0);
}

// Parse int safely
static int parse_int(const char *str)
{
    return atoi(str);
}

// Load .env file and parse key=value lines
int load_env(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Failed to open env file");
        return 1;
    }

    char line[512];
    while (fgets(line, sizeof(line), file))
    {
        if (line[0] == '#' || line[0] == '\n')
            continue;

        char *equals = strchr(line, '=');
        if (!equals)
            continue;

        *equals = 0;
        char *key = line;
        char *value = equals + 1;

        trim(key);
        trim(value);

        // Remove surrounding quotes if any
        size_t len = strlen(value);
        if (len >= 2 && value[0] == '"' && value[len - 1] == '"')
        {
            value[len - 1] = 0;
            memmove(value, value + 1, len - 1);
        }

        if (strcmp(key, "BOT_TOKEN") == 0)
        {
            strncpy(BOT_TOKEN, value, sizeof(BOT_TOKEN) - 1);
            BOT_TOKEN[sizeof(BOT_TOKEN) - 1] = '\0';
        }
        else if (strcmp(key, "CHAT_ID") == 0)
        {
            strncpy(CHAT_ID, value, sizeof(CHAT_ID) - 1);
            CHAT_ID[sizeof(CHAT_ID) - 1] = '\0';
        }
        else if (strcmp(key, "COM_PORT") == 0)
        {
            strncpy(COM_PORT, value, sizeof(COM_PORT) - 1);
            COM_PORT[sizeof(COM_PORT) - 1] = '\0';
        }
        else if (strcmp(key, "RECORDING_DIRECTORY") == 0)
        {
            strncpy(RECORDING_DIRECTORY, value, sizeof(RECORDING_DIRECTORY) - 1);
            RECORDING_DIRECTORY[sizeof(RECORDING_DIRECTORY) - 1] = '\0';
        }
        else if (strcmp(key, "AUDIO_INPUT_DEVICE") == 0)
        {
            strncpy(AUDIO_INPUT_DEVICE, value, sizeof(AUDIO_INPUT_DEVICE) - 1);
            AUDIO_INPUT_DEVICE[sizeof(AUDIO_INPUT_DEVICE) - 1] = '\0';

            Pa_Initialize();
            AUDIO_INPUT_DEVICE_ID = get_device_index_by_name(AUDIO_INPUT_DEVICE, 1);
            Pa_Terminate();
        }
        else if (strcmp(key, "AUDIO_OUTPUT_DEVICE") == 0)
        {
            strncpy(AUDIO_OUTPUT_DEVICE, value, sizeof(AUDIO_OUTPUT_DEVICE) - 1);
            AUDIO_OUTPUT_DEVICE[sizeof(AUDIO_OUTPUT_DEVICE) - 1] = '\0';

            Pa_Initialize();
            AUDIO_OUTPUT_DEVICE_ID = get_device_index_by_name(AUDIO_OUTPUT_DEVICE, 0);
            Pa_Terminate();
        }
        else if (strcmp(key, "USER_NAME") == 0)
        {
            strncpy(USER_NAME, value, sizeof(USER_NAME) - 1);
            USER_NAME[sizeof(USER_NAME) - 1] = '\0';
        }
        else if (strcmp(key, "WORKDIR") == 0)
        {
            strncpy(WORKDIR, value, sizeof(WORKDIR) - 1);
            WORKDIR[sizeof(WORKDIR) - 1] = '\0';
        }
        else if (strcmp(key, "RECORDER_CMD") == 0)
        {
            strncpy(RECORDER_CMD, value, sizeof(RECORDER_CMD) - 1);
            RECORDER_CMD[sizeof(RECORDER_CMD) - 1] = '\0';
        }
        else if (strcmp(key, "REPO_BRANCH") == 0)
        {
            strncpy(REPO_BRANCH, value, sizeof(REPO_BRANCH) - 1);
            REPO_BRANCH[sizeof(REPO_BRANCH) - 1] = '\0';
        }
        else if (strcmp(key, "AMPLITUDE_THRESHOLD") == 0)
        {
            AMPLITUDE_THRESHOLD = parse_int(value);
        }
        else if (strcmp(key, "CHUNK_SIZE") == 0)
        {
            CHUNK_SIZE = parse_int(value);
        }
        else if (strcmp(key, "DEBUG_AMPLITUDE") == 0)
        {
            DEBUG_AMPLITUDE = parse_bool(value);
        }
        else if (strcmp(key, "LIVE_LISTEN") == 0)
        {
            LIVE_LISTEN = parse_bool(value);
        }
        else if (strcmp(key, "EXTRA_TEXT") == 0)
        {
            strncpy(EXTRA_TEXT, value, sizeof(EXTRA_TEXT) - 1);
            EXTRA_TEXT[sizeof(EXTRA_TEXT) - 1] = '\0';
        }
        else if (strcmp(key, "SILENCE_THRESHOLD") == 0)
        {
            SILENCE_THRESHOLD = parse_int(value);
        }
        else if (strcmp(key, "REMOVE_LAST_SECONDS") == 0)
        {
            REMOVE_LAST_SECONDS = parse_int(value);
        }
    }

    fclose(file);

    // After loading, parse CHAT_ID string into CHAT_IDS array
    parse_chat_id_array(CHAT_ID);

    return 0;
}
