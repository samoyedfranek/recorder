#include <ctype.h>

int LIVE_LISTEN = 0;  // Add this global variable in your config.h or above load_config

// Helper to lowercase a string in-place
void str_to_lower(char *str)
{
    for (; *str; ++str)
        *str = (char)tolower(*str);
}

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
            else if (strcmp(key, "AUDIO_OUTPUT_DEVICE") == 0)
                AUDIO_OUTPUT_DEVICE = strdup(value);
            else if (strcmp(key, "LIVE_LISTEN") == 0)
            {
                // lowercase value for case-insensitive compare
                str_to_lower(value);
                if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0)
                    LIVE_LISTEN = 1;
                else if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0)
                    LIVE_LISTEN = 0;
                else
                    fprintf(stderr, "Warning: Invalid LIVE_LISTEN value: %s. Using default 0.\n", value);
            }
        }
    }

    fclose(file);
}
