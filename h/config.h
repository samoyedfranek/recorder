#ifndef CONFIG_H
#define CONFIG_H

#define MAX_LINE_LENGTH 512
#define MAX_ENV_VARS 20

typedef struct
{
    char BOT_TOKEN[256];
    char CHAT_ID[256];
    char COM_PORT[256];
    char RECORDING_DIRECTORY[256];
    int AUDIO_INPUT_DEVICE;
    int AUDIO_OUTPUT_DEVICE;
    char USER_NAME[256];
    char WORKDIR[256];
    char RECORDER_CMD[256];
    char REPO_BRANCH[64];
    int AMPLITUDE_THRESHOLD;
    int DEBUG_AMPLITUDE; // 0 or 1
    int LIVE_LISTEN;     // 0 or 1 (can be ignored if not used)
    char EXTRA_TEXT[256];
} CONFIG;

// Load .env file and populate CONFIG struct, returns 0 on success, -1 on failure
int LOAD_CONFIG(const char *filename, CONFIG *config);

#endif
