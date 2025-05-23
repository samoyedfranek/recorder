#ifndef CONFIG_H
#define CONFIG_H

#define MAX_LINE_LENGTH 512
#define MAX_ENV_VARS 20

typedef struct
{
    char bot_token[256];
    char chat_id[256];
    char com_port[256];
    char recording_directory[256];
    int audio_input_device;
    int audio_output_device;
    char user_name[256];
    char workdir[256];
    char recorder_cmd[256];
    char repo_branch[64];
    int amplitude_threshold;
    int debug_amplitude; // 0 or 1
    int live_listen;     // 0 or 1 (can be ignored if not used)
    char extra_text[256];
} Config;

// Load .env file and populate Config struct, returns 0 on success, -1 on failure
int load_config(const char *filename, Config *config);

#endif
