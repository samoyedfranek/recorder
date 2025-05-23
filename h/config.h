#ifndef CONFIG_H
#define CONFIG_H

#define MAX_LINE_LENGTH 512
#define MAX_ENV_VARS 20
#define MAX_CHAT_IDS 10

typedef struct
{
    char BOT_TOKEN[256];
    char *CHAT_IDS[MAX_CHAT_IDS]; // NULL-terminated array
    char COM_PORT[256];
    char RECORDING_DIRECTORY[256];
    int AUDIO_INPUT_DEVICE;
    int AUDIO_OUTPUT_DEVICE;
    char USER_NAME[256];
    char WORKDIR[256];
    char RECORDER_CMD[256];
    char REPO_BRANCH[64];
    int AMPLITUDE_THRESHOLD;
    int DEBUG_AMPLITUDE;
    int LIVE_LISTEN;
    char EXTRA_TEXT[256];
} CONFIG;

extern CONFIG config;

int load_config(const char *filename);

#endif
