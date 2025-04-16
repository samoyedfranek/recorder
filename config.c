#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "h/config.h"

char *BOT_TOKEN = NULL;
char *COM_PORT = NULL;
char *RECORDING_DIRECTORY = NULL;
int AUDIO_INPUT_DEVICE = 0;
char **CHAT_IDS = NULL;

void load_config(const char *env_file)
{

    FILE *file = fopen(env_file, "r");
    if (!file)
    {
        fprintf(stderr, "Error opening .env file\n");
        exit(1);
    }

    char line[256];
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
                BOT_TOKEN = strdup(value);
            else if (strcmp(key, "CHAT_ID") == 0)
            {

                int count = 0;
                char *tmp = value;
                while (*tmp)
                {
                    if (*tmp == ',')
                        count++;
                    tmp++;
                }
                count++;

                CHAT_IDS = (char **)malloc(sizeof(char *) * (count + 1));
                if (!CHAT_IDS)
                {
                    fprintf(stderr, "Memory allocation failed for CHAT_IDS\n");
                    exit(1);
                }

                char *token = strtok(value, ",");
                for (int i = 0; i < count; i++)
                {
                    CHAT_IDS[i] = strdup(token);
                    token = strtok(NULL, ",");
                }
                CHAT_IDS[count] = NULL;
            }
            else if (strcmp(key, "COM_PORT") == 0)
                COM_PORT = strdup(value);
            else if (strcmp(key, "RECORDING_DIRECTORY") == 0)
                RECORDING_DIRECTORY = strdup(value);
            else if (strcmp(key, "AUDIO_INPUT_DEVICE") == 0)
                AUDIO_INPUT_DEVICE = atoi(value);
        }
    }

    fclose(file);
}
