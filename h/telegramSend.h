#ifndef TELEGRAM_SENDER_H
#define TELEGRAM_SENDER_H

#include <stdio.h>

void get_current_datetime(char *datetime_str, size_t size);

int send_to_telegram(const char *file_path, const char *bot_token, char **chat_ids);
int send_telegram_status(const char *bot_token, char **chat_ids, const char *message);
#endif
