#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <uv.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include <jack/jack.h>
#include <fcntl.h>
#include <errno.h>
#include "h/telegramSend.h"
#include "h/recordAudio.h"
#include "h/config.h"

static void silent_alsa_error(const char *file, int line, const char *function,
                              int err, const char *fmt, ...) {}

static void silent_jack_error(const char *msg) {}

static void silent_jack_info(const char *msg) {}

__attribute__((constructor)) static void suppress_audio_errors(void)
{
    snd_lib_error_set_handler(silent_alsa_error);
    jack_set_error_function(silent_jack_error);
    jack_set_info_function(silent_jack_info);
}

int create_directory_if_not_exists(const char *dir_path)
{
    struct stat st = {0};
    if (stat(dir_path, &st) == -1)
    {
        if (mkdir(dir_path, 0700) == -1)
        {
            perror("Failed to create directory");
            return -1;
        }
        printf("Directory created: %s\n", dir_path);
    }
    return 0;
}

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
            if (S_ISREG(file_stat.st_mode))
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

void on_new_file_created(uv_fs_event_t *handle, const char *filename, int events, int status)
{
    if (filename == NULL)
        return;

    if ((events & UV_RENAME) || (events & UV_CHANGE))
    {

        const char *directory = (const char *)handle->data;
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", directory, filename);

        sleep(1);

        struct stat file_stat;
        if (stat(full_path, &file_stat) != 0 || !S_ISREG(file_stat.st_mode))
        {

            return;
        }

        printf("New file detected: %s\n", full_path);
        send_to_telegram(full_path, BOT_TOKEN, CHAT_IDS);
    }
}

void *monitor_directory_thread(void *arg)
{
    const char *directory = (const char *)arg;
    uv_loop_t loop;

    if (uv_loop_init(&loop))
    {
        fprintf(stderr, "Error initializing uv loop\n");
        return NULL;
    }

    uv_fs_event_t fs_event;
    int status = uv_fs_event_init(&loop, &fs_event);
    if (status != 0)
    {
        fprintf(stderr, "Error initializing fs event: %s\n", uv_strerror(status));
        uv_loop_close(&loop);
        return NULL;
    }

    fs_event.data = (void *)directory;

    status = uv_fs_event_start(&fs_event, on_new_file_created, directory, UV_FS_EVENT_RECURSIVE);
    if (status != 0)
    {
        fprintf(stderr, "Error starting file event monitoring: %s\n", uv_strerror(status));
        uv_fs_event_stop(&fs_event);
        uv_loop_close(&loop);
        return NULL;
    }

    printf("Monitoring directory: %s\n", directory);
    uv_run(&loop, UV_RUN_DEFAULT);

    uv_fs_event_stop(&fs_event);
    uv_loop_close(&loop);
    return NULL;
}

void *recorder_thread(void *arg)
{
    printf("Starting recording on device with COM port %s\n", COM_PORT);
    send_telegram_status(BOT_TOKEN, CHAT_IDS, "Rozpoczynanie nagrywania");
    recorder(COM_PORT);
    return NULL;
}

int main(void)
{
    snd_lib_error_set_handler(silent_alsa_error);
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    load_config(".env");

    if (create_directory_if_not_exists(RECORDING_DIRECTORY) != 0)
    {
        return 1;
    }

    pthread_t recorder_thread_id, monitor_thread_id;

    send_existing_files(RECORDING_DIRECTORY);

    if (pthread_create(&recorder_thread_id, NULL, recorder_thread, NULL) != 0)
    {
        perror("Failed to create recorder thread");
        return 1;
    }

    if (pthread_create(&monitor_thread_id, NULL, monitor_directory_thread, (void *)RECORDING_DIRECTORY) != 0)
    {
        perror("Failed to create monitor thread");
        return 1;
    }

    pthread_join(recorder_thread_id, NULL);
    pthread_join(monitor_thread_id, NULL);

    printf("All files processed successfully.\n");
    return 0;
}
