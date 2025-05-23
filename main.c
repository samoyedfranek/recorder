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

#define MAX_QUEUE_SIZE 100
char *playback_queue[MAX_QUEUE_SIZE];
int queue_start = 0, queue_end = 0;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

static void silent_alsa_error(const char *file, int line, const char *function, int err, const char *fmt, ...) {}
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

void enqueue_file_for_playback(const char *filepath)
{
    pthread_mutex_lock(&queue_mutex);
    int next = (queue_end + 1) % MAX_QUEUE_SIZE;
    if (next != queue_start)
    {
        playback_queue[queue_end] = strdup(filepath);
        queue_end = next;
        pthread_cond_signal(&queue_cond);
    }
    else
    {
        fprintf(stderr, "Playback queue is full. Dropping file: %s\n", filepath);
    }
    pthread_mutex_unlock(&queue_mutex);
}

char *dequeue_file_for_playback()
{
    pthread_mutex_lock(&queue_mutex);
    while (queue_start == queue_end)
    {
        pthread_cond_wait(&queue_cond, &queue_mutex);
    }

    char *filepath = playback_queue[queue_start];
    queue_start = (queue_start + 1) % MAX_QUEUE_SIZE;
    pthread_mutex_unlock(&queue_mutex);
    return filepath;
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
            return;

        printf("New file detected: %s\n", full_path);
        enqueue_file_for_playback(full_path);
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

void *playback_thread(void *arg)
{
    (void)arg;

    create_directory_if_not_exists("./play");

    while (1)
    {
        char *filepath = dequeue_file_for_playback();
        if (!filepath)
            continue;

        char filename[256];
        snprintf(filename, sizeof(filename), "%s", strrchr(filepath, '/') + 1);

        char playpath[512];
        snprintf(playpath, sizeof(playpath), "./play/%s", filename);

        char copy_cmd[1024];
        snprintf(copy_cmd, sizeof(copy_cmd), "cp '%s' '%s'", filepath, playpath);
        if (system(copy_cmd) != 0)
        {
            fprintf(stderr, "Failed to copy %s to %s\n", filepath, playpath);
            free(filepath);
            continue;
        }

        char play_cmd[1024];
        snprintf(play_cmd, sizeof(play_cmd), "aplay -D plughw:%s '%s'", AUDIO_OUTPUT_DEVICE, playpath);
        if (system(play_cmd) != 0)
        {
            fprintf(stderr, "Playback failed for %s\n", playpath);
        }
        else
        {
            printf("Playback finished: %s\n", playpath);
            unlink(playpath);
        }

        free(filepath);
    }
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
    suppress_audio_errors();
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    load_config(".env");

    if (create_directory_if_not_exists(RECORDING_DIRECTORY) != 0)
    {
        return 1;
    }

    pthread_t recorder_thread_id, monitor_thread_id, playback_thread_id;

    if (pthread_create(&recorder_thread_id, NULL, recorder_thread, NULL) != 0 ||
        pthread_create(&monitor_thread_id, NULL, monitor_directory_thread, (void *)RECORDING_DIRECTORY) != 0)
    {
        perror("Failed to create thread");
        return 1;
    }

    if (LIVE_LISTEN)
    {
        if (pthread_create(&playback_thread_id, NULL, playback_thread, NULL) != 0)
        {
            perror("Failed to create playback thread");
            return 1;
        }
    }

    pthread_join(recorder_thread_id, NULL);
    pthread_join(monitor_thread_id, NULL);

    if (LIVE_LISTEN)
    {
        pthread_join(playback_thread_id, NULL);
    }

    return 0;
}
