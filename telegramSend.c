#include "h/telegramSend.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <regex.h>
#include <stdio.h>
#include <errno.h>

// Helper function to get current date and time in the desired format
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
        // Extract base name (e.g. "OSP POWIAT")
        snprintf(base_name, base_size, "%.*s", (int)(matches[1].rm_eo - matches[1].rm_so), file_path + matches[1].rm_so);
        // Extract timestamp string (e.g. "20250327_190129")
        snprintf(timestamp, time_size, "%.*s", (int)(matches[2].rm_eo - matches[2].rm_so), file_path + matches[2].rm_so);

        // Convert timestamp from format "YYYYMMDD_HHMMSS" to "YYYY-MM-DD HH:MM:SS"
        char formatted_timestamp[32];
        sprintf(formatted_timestamp, "%.4s-%.2s-%.2s %.2s:%.2s:%.2s",
                timestamp,       // YYYY
                timestamp + 4,   // MM
                timestamp + 6,   // DD
                timestamp + 9,   // HH (skip the underscore at index 8)
                timestamp + 11,  // MM
                timestamp + 13); // SS
        strncpy(timestamp, formatted_timestamp, time_size);
    }
    else
    {
        // If regex match fails, just copy the file name to base_name
        strncpy(base_name, file_path, base_size - 1);
        base_name[base_size - 1] = '\0';
        timestamp[0] = '\0'; // No timestamp found
    }

    regfree(&regex);
}

int send_to_telegram(const char *file_path, const char *bot_token, char **chat_ids)
{
    CURL *curl;
    CURLcode res;
    char url[256];
    char base_name[256];
    char timestamp[32];

    // Extract base name and timestamp from the file path.
    // For a filename like "OSP POWIAT_20250327_190129.wav", base_name becomes "OSP POWIAT"
    extract_timestamp(file_path, base_name, timestamp, sizeof(base_name), sizeof(timestamp));

    // Build a new file name from base_name (only letters from start) with .wav extension.
    // This renaming removes the date/timestamp portion from the file name.
    char new_file_path[512];
    snprintf(new_file_path, sizeof(new_file_path), "%s.wav", base_name);

    // Rename the file on disk
    if (rename(file_path, new_file_path) != 0)
    {
        fprintf(stderr, "Failed to rename file from %s to %s: %s\n", file_path, new_file_path, strerror(errno));
        return 0;
    }

    // Use the new file path for sending
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendAudio", bot_token);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (!curl)
    {
        return 0; // Initialization failed
    }

    for (int i = 0; chat_ids[i] != NULL; i++)
    {
        struct curl_mime *mime;
        struct curl_mimepart *part;

        mime = curl_mime_init(curl);

        // Add the audio file as part of the request
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "audio");
        curl_mime_filedata(part, new_file_path); // Use the renamed file

        // Add chat_id as part of the request
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "chat_id");
        curl_mime_data(part, chat_ids[i], CURL_ZERO_TERMINATED);

        // Escape MarkdownV2 special characters in the timestamp (if available)
        char escaped_caption[512];
        if (timestamp[0] != '\0')
        {
            escape_markdown_v2(escaped_caption, timestamp, sizeof(escaped_caption));

            // Prepare the caption using the escaped timestamp and an additional message
            char caption[512];
            snprintf(caption, sizeof(caption), "%s\n*COŚ SIĘ DZIEJE*", escaped_caption);

            // Add the caption part
            part = curl_mime_addpart(mime);
            curl_mime_name(part, "caption");
            curl_mime_data(part, caption, CURL_ZERO_TERMINATED);
        }

        // Add parse_mode as a separate parameter
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "parse_mode");
        curl_mime_data(part, "MarkdownV2", CURL_ZERO_TERMINATED);

        // Set the URL and the mime-encoded request
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

        // Perform the request
        res = curl_easy_perform(curl);
        curl_mime_free(mime);

        // Check for errors
        if (res != CURLE_OK)
        {
            fprintf(stderr, "Failed to send file %s to chat %s: %s\n", new_file_path, chat_ids[i], curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            return 0; // Exit on failure
        }

        // Remove the file after sending
        if (remove(new_file_path) != 0)
        {
            fprintf(stderr, "Failed to remove file %s: %s\n", new_file_path, strerror(errno));
        }
    }

    curl_easy_cleanup(curl); // Clean up the CURL instance
    return 1;                // Success
}

// Helper function to escape MarkdownV2 special characters
void escape_markdown_v2(char *dest, const char *src, size_t size)
{
    size_t i = 0, j = 0;
    while (src[i] != '\0' && j < size - 1)
    {
        // Check if character needs escaping
        if (src[i] == '_' || src[i] == '*' || src[i] == '`' || src[i] == '[' || src[i] == ']' ||
            src[i] == '(' || src[i] == ')' || src[i] == '>' || src[i] == '#' || src[i] == '+' ||
            src[i] == '-' || src[i] == '.' || src[i] == '!')
        {
            if (j + 2 < size)
            {
                dest[j++] = '\\'; // Escape the character
                dest[j++] = src[i];
            }
        }
        else
        {
            dest[j++] = src[i];
        }
        i++;
    }
    dest[j] = '\0'; // Null terminate the string
}

// --- send_telegram_status function ---
int send_telegram_status(const char *bot_token, char **chat_ids, const char *message)
{
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
    char url[256];
    char message_escaped[1024];

    // Escape special characters in the message for MarkdownV2
    escape_markdown_v2(message_escaped, message, sizeof(message_escaped));

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

    // Set the Content-Type header for JSON
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Loop through the chat IDs array and send the status message to each
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
