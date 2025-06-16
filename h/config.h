#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

// Config variables (extern so they can be accessed from other files)
extern char BOT_TOKEN[256];
extern char CHAT_ID[256];
extern char *CHAT_IDS[21];
extern char COM_PORT[128];
extern char RECORDING_DIRECTORY[128];
extern int AUDIO_INPUT_DEVICE_ID;
extern int AUDIO_OUTPUT_DEVICE_ID;
extern char USER_NAME[64];
extern char WORKDIR[128];
extern char RECORDER_CMD[256];
extern char REPO_BRANCH[64];
extern int AMPLITUDE_THRESHOLD;
extern int CHUNK_SIZE;
extern bool DEBUG_AMPLITUDE;
extern bool LIVE_LISTEN;
extern char EXTRA_TEXT[64];
extern int SILENCE_THRESHOLD;
extern int REMOVE_LAST_SECONDS;

// Load env file, return 0 on success, non-zero on failure
int load_env(const char *filename);

#endif // CONFIG_H
