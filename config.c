#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "h/config.h"

CONFIG config;

int load_config(const char *env_file)
{
    FILE *file = fopen(env_file, "r");
    if (!file)
    {
        fprintf(stderr, "Error opening .env file\n");
        return -1;
    }

    char line[512];
    while (fgets(line, sizeof(line), file))
    {
        line[strcspn(line, "\n")] = 0;

        if (line[0] == '#' || line[0] == '\0')
            continue;

        char *key = strtok(line, "=");
        char *value = strtok(NULL, "=");

        if (key && value)
        {
            if (strcmp(key, "BOT_TOKEN") == 0)
                strncpy(config.BOT_TOKEN, value, sizeof(config.BOT_TOKEN));
            else if (strcmp(key, "CHAT_ID") == 0)
            {
                int count = 0;
                char *token = strtok(value, ",");
                while (token && count < MAX_CHAT_IDS - 1)
                {
                    config.CHAT_IDS[count++] = strdup(token);
                    token = strtok(NULL, ",");
                }
                config.CHAT_IDS[count] = NULL;
            }
            else if (strcmp(key, "COM_PORT") == 0)
                strncpy(config.COM_PORT, value, sizeof(config.COM_PORT));
            else if (strcmp(key, "RECORDING_DIRECTORY") == 0)
                strncpy(config.RECORDING_DIRECTORY, value, sizeof(config.RECORDING_DIRECTORY));
            else if (strcmp(key, "AUDIO_INPUT_DEVICE") == 0)
                config.AUDIO_INPUT_DEVICE = atoi(value);
            else if (strcmp(key, "AUDIO_OUTPUT_DEVICE") == 0)
                config.AUDIO_OUTPUT_DEVICE = atoi(value);
            else if (strcmp(key, "USER_NAME") == 0)
                strncpy(config.USER_NAME, value, sizeof(config.USER_NAME));
            else if (strcmp(key, "WORKDIR") == 0)
                strncpy(config.WORKDIR, value, sizeof(config.WORKDIR));
            else if (strcmp(key, "RECORDER_CMD") == 0)
                strncpy(config.RECORDER_CMD, value, sizeof(config.RECORDER_CMD));
            else if (strcmp(key, "REPO_BRANCH") == 0)
                strncpy(config.REPO_BRANCH, value, sizeof(config.REPO_BRANCH));
            else if (strcmp(key, "AMPLITUDE_THRESHOLD") == 0)
                config.AMPLITUDE_THRESHOLD = atoi(value);
            else if (strcmp(key, "DEBUG_AMPLITUDE") == 0)
                config.DEBUG_AMPLITUDE = atoi(value);
            else if (strcmp(key, "LIVE_LISTEN") == 0)
                config.LIVE_LISTEN = atoi(value);
            else if (strcmp(key, "EXTRA_TEXT") == 0)
                strncpy(config.EXTRA_TEXT, value, sizeof(config.EXTRA_TEXT));
        }
    }

    fclose(file);
    return 0;
}
