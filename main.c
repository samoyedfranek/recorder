#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <uv.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include "h/telegramSend.h"
#include "h/recordAudio.h"
#include "h/config.h"
#include "h/globals.h"

volatile sig_atomic_t keepRunning = 1;

// Signal handler
void handle_signal(int sig)
{
    keepRunning = 0;
    printf("Signal %d received, shutting down...\n", sig);
    fflush(stdout);
}

// Function to send file to Telegram
void send_existing_files(const char *directory)
{
    DIR *dir;
    struct dirent *entry;

    if ((dir = opendir(directory)) == NULL)
    {
        perror("Failed to open directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char file_path[512];
        snprintf(file_path, sizeof(file_path), "%s/%s", directory, entry->d_name);

        struct stat file_stat;
        if (stat(file_path, &file_stat) == 0)
        {
            if (S_ISREG(file_stat.st_mode)) // Ensure it's a regular file
            {
                printf("Sending existing file: %s\n", file_path);
                send_to_telegram(file_path, BOT_TOKEN, CHAT_IDS);
            }
        }
        else
        {
            perror("Error retrieving file info");
        }
    }

    closedir(dir);
}

// Libuv directory change event callback
void on_new_file_created(uv_fs_event_t *handle, const char *filename, int events, int status)
{
    if (filename == NULL)
        return;

    // Process events for rename or change
    if (events & UV_RENAME || events & UV_CHANGE)
    {
        // Retrieve the monitored directory from the handle data
        const char *directory = (const char *)handle->data;
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", directory, filename);

        // Optional: wait briefly to ensure file is fully written
        sleep(1);

        // Check if the file exists and is a regular file
        struct stat file_stat;
        if (stat(full_path, &file_stat) != 0 || !S_ISREG(file_stat.st_mode))
        {
            // If not a regular file, skip it
            return;
        }

        printf("New file detected: %s\n", full_path);
        send_to_telegram(full_path, BOT_TOKEN, CHAT_IDS);
    }
}

// Monitor directory using libuv
void monitor_directory(const char *directory)
{
    uv_loop_t *loop = uv_default_loop();
    uv_fs_event_t fs_event;

    printf("Initializing directory monitoring for: %s\n", directory);

    int status = uv_fs_event_init(loop, &fs_event);
    if (status != 0)
    {
        fprintf(stderr, "Error initializing fs event: %s\n", uv_strerror(status));
        return;
    }

    // Store the directory path in the event handle's data for later use
    fs_event.data = (void *)directory;

    status = uv_fs_event_start(&fs_event, on_new_file_created, directory, UV_FS_EVENT_RECURSIVE);
    if (status != 0)
    {
        fprintf(stderr, "Error starting file event monitoring: %s\n", uv_strerror(status));
        return;
    }

    printf("Monitoring directory: %s\n", directory);
    uv_run(loop, UV_RUN_DEFAULT);
    uv_fs_event_stop(&fs_event);
}

// Recorder function running in a separate thread
void *recorder_thread(void *arg)
{
    printf("Starting recording on device with COM port %s\n", COM_PORT);
    send_telegram_status(BOT_TOKEN, CHAT_IDS, "Recording started");
    recorder(COM_PORT);
    return NULL;
}

int main(void)
{
    // Load configuration from .env file
    load_config(".env");

    // Setup signal handlers for SIGINT and SIGTERM
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    pthread_t recorder_thread_id, monitor_thread_id;

    // Send any existing files before monitoring starts
    send_existing_files(RECORDING_DIRECTORY);

    // Create a thread for the recorder, passing COM_PORT as argument
    if (pthread_create(&recorder_thread_id, NULL, (void *(*)(void *))recorder, (void *)COM_PORT) != 0)
    {
        perror("Failed to create recorder thread");
        return 1;
    }

    // Create a thread for directory monitoring, passing RECORDING_DIRECTORY as argument
    if (pthread_create(&monitor_thread_id, NULL, (void *(*)(void *))monitor_directory, (void *)RECORDING_DIRECTORY) != 0)
    {
        perror("Failed to create monitor thread");
        return 1;
    }

    // Wait for both threads to finish
    pthread_join(recorder_thread_id, NULL);
    pthread_join(monitor_thread_id, NULL);

    printf("All files processed successfully.\n");
    fflush(stdout);
    return 0;
}