#include "h/config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// Trim leading/trailing whitespace
static void trim(char *str)
{
    // Trim leading spaces
    while (isspace((unsigned char)*str))
        str++;

    // Trim trailing spaces
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;
    *(end + 1) = '\0';
}

int load_config(const char *filename, Config *config)
{
    // Set defaults
    memset(config, 0, sizeof(Config));
    strcpy(config->bot_token, "");
    strcpy(config->chat_id, "");
    strcpy(config->com_port, "/dev/ttyACM0");
    strcpy(config->recording_directory, "./recordings");
    config->audio_input_device = 0;
    config->audio_output_device = 0;
    strcpy(config->user_name, "");
    strcpy(config->workdir, "");
    strcpy(config->recorder_cmd, "");
    strcpy(config->repo_branch, "main");
    config->amplitude_threshold = 300;
    config->debug_amplitude = 0;
    config->live_listen = 0;
    strcpy(config->extra_text, "");

    FILE *f = fopen(filename, "r");
    if (!f)
        return -1;

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), f))
    {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n')
            continue;

        char *eq = strchr(line, '=');
        if (!eq)
            continue;

        *eq = '\0';
        char *key = line;
        char *value = eq + 1;

        trim(key);
        trim(value);

        if (strcmp(key, "BOT_TOKEN") == 0)
        {
            strncpy(config->bot_token, value, sizeof(config->bot_token) - 1);
        }
        else if (strcmp(key, "CHAT_ID") == 0)
        {
            strncpy(config->chat_id, value, sizeof(config->chat_id) - 1);
        }
        else if (strcmp(key, "COM_PORT") == 0)
        {
            strncpy(config->com_port, value, sizeof(config->com_port) - 1);
        }
        else if (strcmp(key, "RECORDING_DIRECTORY") == 0)
        {
            strncpy(config->recording_directory, value, sizeof(config->recording_directory) - 1);
        }
        else if (strcmp(key, "AUDIO_INPUT_DEVICE") == 0)
        {
            config->audio_input_device = atoi(value);
        }
        else if (strcmp(key, "AUDIO_OUTPUT_DEVICE") == 0)
        {
            config->audio_output_device = atoi(value);
        }
        else if (strcmp(key, "USER_NAME") == 0)
        {
            strncpy(config->user_name, value, sizeof(config->user_name) - 1);
        }
        else if (strcmp(key, "WORKDIR") == 0)
        {
            strncpy(config->workdir, value, sizeof(config->workdir) - 1);
        }
        else if (strcmp(key, "RECORDER_CMD") == 0)
        {
            strncpy(config->recorder_cmd, value, sizeof(config->recorder_cmd) - 1);
        }
        else if (strcmp(key, "REPO_BRANCH") == 0)
        {
            strncpy(config->repo_branch, value, sizeof(config->repo_branch) - 1);
        }
        else if (strcmp(key, "AMPLITUDE_THRESHOLD") == 0)
        {
            config->amplitude_threshold = atoi(value);
        }
        else if (strcmp(key, "DEBUG_AMPLITUDE") == 0)
        {
            if (strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0)
                config->debug_amplitude = 1;
            else
                config->debug_amplitude = 0;
        }
        else if (strcmp(key, "LIVE_LISTEN") == 0)
        {
            if (strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0)
                config->live_listen = 1;
            else
                config->live_listen = 0;
        }
        else if (strcmp(key, "EXTRA_TEXT") == 0)
        {
            strncpy(config->extra_text, value, sizeof(config->extra_text) - 1);
        }
    }

    fclose(f);
    return 0;
}
