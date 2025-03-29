#ifndef CONFIG_H
#define CONFIG_H

extern char *BOT_TOKEN;
extern char *COM_PORT;
extern char *RECORDING_DIRECTORY;
extern char **CHAT_IDS;

void load_config(const char *env_file);

#endif // CONFIG_H
