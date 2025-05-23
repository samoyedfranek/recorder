#ifndef CONFIG_H
#define CONFIG_H

extern char *BOT_TOKEN;
extern char *COM_PORT;
extern char *RECORDING_DIRECTORY;
extern int AUDIO_INPUT_DEVICE;
extern char **CHAT_IDS;
extern char *AUDIO_OUTPUT_DEVICE;
extern int LIVE_LISTEN;

void load_config(const char *env_file);

#endif // CONFIG_H
