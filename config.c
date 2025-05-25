#include "h/config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

// Define variables with some default values or empty
char BOT_TOKEN[256] = "";
char CHAT_ID[256] = "";
char COM_PORT[128] = "";
char RECORDING_DIRECTORY[128] = "";
int AUDIO_INPUT_DEVICE = 0;
int AUDIO_OUTPUT_DEVICE = 0;
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

// ----- Chat IDs parsing stuff -----
#define MAX_CHAT_IDS 20
char *CHAT_IDS[MAX_CHAT_IDS + 1];
int chat_ids_count = 0;

void parse_chat_id_array(const char *chat_id_str) {
    // No free_chat_ids() called, so no free of previous memory
    // Note: memory leak if parse_chat_id_array called multiple times!

    if (!chat_id_str || strlen(chat_id_str) == 0) {
        chat_ids[0] = NULL;
        chat_ids_count = 0;
        return;
    }

    char temp[512];
    strncpy(temp, chat_id_str, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *token = strtok(temp, ",");
    chat_ids_count = 0;

    while (token != NULL && chat_ids_count < MAX_CHAT_IDS) {
        // trim leading spaces
        while (*token == ' ') token++;

        chat_ids[chat_ids_count] = strdup(token);
        if (!chat_ids[chat_ids_count]) {
            fprintf(stderr, "Memory allocation failed for chat_id\n");
            break;
        }

        chat_ids_count++;
        token = strtok(NULL, ",");
    }

    chat_ids[chat_ids_count] = NULL; // NULL terminate array
}

// ----- Helper to trim leading and trailing whitespace -----
static void trim(char *str) {
    char *end;

    while(isspace((unsigned char)*str)) str++;

    if(*str == 0)
        return;

    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    *(end+1) = 0;
}

// ----- Parse boolean from string -----
static bool parse_bool(const char *str) {
    return (strcmp(str, "1") == 0 || strcasecmp(str, "true") == 0);
}

// ----- Parse int safely -----
static int parse_int(const char *str) {
    return atoi(str);
}

// ----- Load .env file and parse key=value lines -----
int load_env(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open env file");
        return 1;
    }

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        char *equals = strchr(line, '=');
        if (!equals) continue;

        *equals = 0;
        char *key = line;
        char *value = equals + 1;

        trim(key);
        trim(value);

        if (value[0] == '"' && value[strlen(value)-1] == '"') {
            value[strlen(value)-1] = 0;
            memmove(value, value+1, strlen(value));
        }

        if (strcmp(key, "BOT_TOKEN") == 0) {
            strncpy(BOT_TOKEN, value, sizeof(BOT_TOKEN)-1);
        } else if (strcmp(key, "CHAT_ID") == 0) {
            strncpy(CHAT_ID, value, sizeof(CHAT_ID)-1);
        } else if (strcmp(key, "COM_PORT") == 0) {
            strncpy(COM_PORT, value, sizeof(COM_PORT)-1);
        } else if (strcmp(key, "RECORDING_DIRECTORY") == 0) {
            strncpy(RECORDING_DIRECTORY, value, sizeof(RECORDING_DIRECTORY)-1);
        } else if (strcmp(key, "AUDIO_INPUT_DEVICE") == 0) {
            AUDIO_INPUT_DEVICE = parse_int(value);
        } else if (strcmp(key, "AUDIO_OUTPUT_DEVICE") == 0) {
            AUDIO_OUTPUT_DEVICE = parse_int(value);
        } else if (strcmp(key, "USER_NAME") == 0) {
            strncpy(USER_NAME, value, sizeof(USER_NAME)-1);
        } else if (strcmp(key, "WORKDIR") == 0) {
            strncpy(WORKDIR, value, sizeof(WORKDIR)-1);
        } else if (strcmp(key, "RECORDER_CMD") == 0) {
            strncpy(RECORDER_CMD, value, sizeof(RECORDER_CMD)-1);
        } else if (strcmp(key, "REPO_BRANCH") == 0) {
            strncpy(REPO_BRANCH, value, sizeof(REPO_BRANCH)-1);
        } else if (strcmp(key, "AMPLITUDE_THRESHOLD") == 0) {
            AMPLITUDE_THRESHOLD = parse_int(value);
        } else if (strcmp(key, "CHUNK_SIZE") == 0) {
            CHUNK_SIZE = parse_int(value);
        } else if (strcmp(key, "DEBUG_AMPLITUDE") == 0) {
            DEBUG_AMPLITUDE = parse_bool(value);
        } else if (strcmp(key, "LIVE_LISTEN") == 0) {
            LIVE_LISTEN = parse_bool(value);
        } else if (strcmp(key, "EXTRA_TEXT") == 0) {
            strncpy(EXTRA_TEXT, value, sizeof(EXTRA_TEXT)-1);
        } else if (strcmp(key, "SILENCE_THRESHOLD") == 0) {
            SILENCE_THRESHOLD = parse_int(value);
        } else if (strcmp(key, "REMOVE_LAST_SECONDS") == 0) {
            REMOVE_LAST_SECONDS = parse_int(value);
        }
    }

    fclose(file);

    // Parse CHAT_ID string into array after loading .env
    parse_chat_id_array(CHAT_ID);

    return 0;
}
