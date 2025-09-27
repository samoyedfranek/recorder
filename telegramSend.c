#include "h/telegramSend.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <regex.h>
#include <stdio.h>
#include <errno.h>
#include "h/config.h"

void get_current_datetime(char *datetime_str, size_t size)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(datetime_str, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

void escape_markdown_v2(char *dest, const char *src, size_t size);

void extract_timestamp(const char *file_path, char *base_name, char *timestamp, size_t base_size, size_t time_size)
{
    const char *pattern = "(.+)_([0-9]{8}_[0-9]{6})\\.wav$";
    regex_t regex;
    regmatch_t matches[3];

    if (regcomp(&regex, pattern, REG_EXTENDED) != 0)
    {
        fprintf(stderr, "Regex compilation failed\n");
        return;
    }

    if (regexec(&regex, file_path, 3, matches, 0) == 0)
    {
        snprintf(base_name, base_size, "%.*s", (int)(matches[1].rm_eo - matches[1].rm_so), file_path + matches[1].rm_so);

        snprintf(timestamp, time_size, "%.*s", (int)(matches[2].rm_eo - matches[2].rm_so), file_path + matches[2].rm_so);

        char formatted_timestamp[32];
        sprintf(formatted_timestamp, "%.4s-%.2s-%.2s %.2s:%.2s:%.2s",
                timestamp,
                timestamp + 4,
                timestamp + 6,
                timestamp + 9,
                timestamp + 11,
                timestamp + 13);
        strncpy(timestamp, formatted_timestamp, time_size);
    }
    else
    {
        strncpy(base_name, file_path, base_size - 1);
        base_name[base_size - 1] = '\0';
        timestamp[0] = '\0';
    }

    regfree(&regex);
}

int send_to_telegram(const char *file_path, const char *bot_token, char **chat_ids, const char *image_path)
{
    CURL *curl;
    CURLcode res;
    char url[256];
    char base_name[256];
    char timestamp[32];

    load_env(".env");

    extract_timestamp(file_path, base_name, timestamp, sizeof(base_name), sizeof(timestamp));

    char new_file_path[512];
    if (strlen(base_name) >= 4 && strcmp(base_name + strlen(base_name) - 4, ".wav") == 0)
    {
        strncpy(new_file_path, base_name, sizeof(new_file_path));
        new_file_path[sizeof(new_file_path) - 1] = '\0';
    }
    else
    {
        snprintf(new_file_path, sizeof(new_file_path), "%s.wav", base_name);
    }

    if (rename(file_path, new_file_path) != 0)
    {
        fprintf(stderr, "Failed to rename file from %s to %s: %s\n", file_path, new_file_path, strerror(errno));
        return 0;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (!curl)
    {
        return 0;
    }

    for (int i = 0; chat_ids[i] != NULL; i++)
    {
        struct curl_mime *mime;
        struct curl_mimepart *part;

        // --- Optional: send image first ---
        if (image_path != NULL && strlen(image_path) > 0)
        {
            snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendPhoto", bot_token);

            mime = curl_mime_init(curl);

            part = curl_mime_addpart(mime);
            curl_mime_name(part, "chat_id");
            curl_mime_data(part, chat_ids[i], CURL_ZERO_TERMINATED);

            part = curl_mime_addpart(mime);
            curl_mime_name(part, "photo");
            curl_mime_filedata(part, image_path);

            // Caption for photo
            if (timestamp[0] != '\0')
            {
                char escaped_caption[512];
                escape_markdown_v2(escaped_caption, timestamp, sizeof(escaped_caption));

                char escaped_extra[256] = "";
                if (EXTRA_TEXT[0] != '\0')
                    escape_markdown_v2(escaped_extra, EXTRA_TEXT, sizeof(escaped_extra));

                char caption[1024];
                if (escaped_extra[0] != '\0')
                    snprintf(caption, sizeof(caption), "%s\n*COŚ SIĘ DZIEJE*\n%s", escaped_caption, escaped_extra);
                else
                    snprintf(caption, sizeof(caption), "%s\n*COŚ SIĘ DZIEJE*", escaped_caption);

                part = curl_mime_addpart(mime);
                curl_mime_name(part, "caption");
                curl_mime_data(part, caption, CURL_ZERO_TERMINATED);

                part = curl_mime_addpart(mime);
                curl_mime_name(part, "parse_mode");
                curl_mime_data(part, "MarkdownV2", CURL_ZERO_TERMINATED);
            }

            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
            res = curl_easy_perform(curl);
            curl_mime_free(mime);

            if (res != CURLE_OK)
            {
                fprintf(stderr, "Failed to send photo to chat %s: %s\n", chat_ids[i], curl_easy_strerror(res));
                // Don’t return, just continue to audio
            }
        }

        // --- Always send audio ---
        snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendAudio", bot_token);

        mime = curl_mime_init(curl);

        part = curl_mime_addpart(mime);
        curl_mime_name(part, "audio");
        curl_mime_filedata(part, new_file_path);

        part = curl_mime_addpart(mime);
        curl_mime_name(part, "chat_id");
        curl_mime_data(part, chat_ids[i], CURL_ZERO_TERMINATED);

        // Optional caption for audio
        if (timestamp[0] != '\0')
        {
            char escaped_caption[512];
            escape_markdown_v2(escaped_caption, timestamp, sizeof(escaped_caption));

            char escaped_extra[256] = "";
            if (EXTRA_TEXT[0] != '\0')
                escape_markdown_v2(escaped_extra, EXTRA_TEXT, sizeof(escaped_extra));

            char caption[1024];
            if (escaped_extra[0] != '\0')
                snprintf(caption, sizeof(caption), "%s\n*COŚ SIĘ DZIEJE*\n%s", escaped_caption, escaped_extra);
            else
                snprintf(caption, sizeof(caption), "%s\n*COŚ SIĘ DZIEJE*", escaped_caption);

            part = curl_mime_addpart(mime);
            curl_mime_name(part, "caption");
            curl_mime_data(part, caption, CURL_ZERO_TERMINATED);

            part = curl_mime_addpart(mime);
            curl_mime_name(part, "parse_mode");
            curl_mime_data(part, "MarkdownV2", CURL_ZERO_TERMINATED);
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
        res = curl_easy_perform(curl);
        curl_mime_free(mime);

        if (res != CURLE_OK)
        {
            fprintf(stderr, "Failed to send audio %s to chat %s: %s\n", new_file_path, chat_ids[i], curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            return 0;
        }
    }

    if (remove(new_file_path) != 0)
    {
        fprintf(stderr, "Failed to remove file %s: %s\n", new_file_path, strerror(errno));
    }

    curl_easy_cleanup(curl);
    return 1;
}

void escape_markdown_v2(char *dest, const char *src, size_t size)
{
    size_t i = 0, j = 0;
    while (src[i] != '\0' && j < size - 1)
    {
        if (src[i] == '_' || src[i] == '*' || src[i] == '`' || src[i] == '[' || src[i] == ']' ||
            src[i] == '(' || src[i] == ')' || src[i] == '>' || src[i] == '#' || src[i] == '+' ||
            src[i] == '-' || src[i] == '.' || src[i] == '!')
        {
            if (j + 2 < size)
            {
                dest[j++] = '\\';
                dest[j++] = src[i];
            }
        }
        else
        {
            dest[j++] = src[i];
        }
        i++;
    }
    dest[j] = '\0';
}

int send_telegram_status(const char *bot_token, char **chat_ids, const char *message)
{
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
    char url[256];
    char message_escaped[1024];

    load_env(".env");

    char full_message[1280];
    if (EXTRA_TEXT[0] != '\0')
    {
        snprintf(full_message, sizeof(full_message), "%s\n%s", message, EXTRA_TEXT);
    }
    else
    {
        strncpy(full_message, message, sizeof(full_message) - 1);
        full_message[sizeof(full_message) - 1] = '\0';
    }

    escape_markdown_v2(message_escaped, full_message, sizeof(message_escaped));

    if (strlen(message_escaped) == 0)
    {
        fprintf(stderr, "Error: Message is empty\n");
        return 0;
    }

    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendMessage", bot_token);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (!curl)
    {
        return 0;
    }

    headers = curl_slist_append(headers, "Content-Type: application/json");

    for (int i = 0; chat_ids[i] != NULL; i++)
    {
        char data[1024];
        snprintf(data, sizeof(data),
                 "{\"chat_id\": \"%s\", \"text\": \"%s\", \"parse_mode\": \"MarkdownV2\"}",
                 chat_ids[i], message_escaped);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "Failed to send message to chat %s: %s\n", chat_ids[i], curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
            return 0;
        }
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    return 1;
}
