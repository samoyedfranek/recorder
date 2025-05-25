#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <portaudio.h>
#include "h/write_wav_file.h"
#include "h/open_serial_port.h"
#include "h/recordAudio.h"
#include "h/config.h"

#define SAMPLE_RATE 48000
#define CHANNELS 1
#define RECORDINGS_DIR "./recordings"
#define FRAMES_PER_BUFFER 512

// Thread-safe simple queue for live listen data (circular buffer)
typedef struct
{
    short buffer[FRAMES_PER_BUFFER * 64]; // Buffer for 64 chunks (~0.6 seconds)
    int writeIndex;
    int readIndex;
    pthread_mutex_t mutex;
} AudioQueue;

static AudioQueue audioQueue;

void initQueue(AudioQueue *q)
{
    q->writeIndex = 0;
    q->readIndex = 0;
    pthread_mutex_init(&q->mutex, NULL);
}

void enqueue(AudioQueue *q, const short *data, int frames)
{
    pthread_mutex_lock(&q->mutex);
    int pos = q->writeIndex * FRAMES_PER_BUFFER;
    memcpy(&q->buffer[pos], data, frames * sizeof(short));
    q->writeIndex = (q->writeIndex + 1) % 64;
    pthread_mutex_unlock(&q->mutex);
}

int dequeue(AudioQueue *q, short *data, int frames)
{
    int empty = 0;
    pthread_mutex_lock(&q->mutex);
    if (q->readIndex == q->writeIndex)
    {
        empty = 1; // buffer empty
    }
    else
    {
        int pos = q->readIndex * FRAMES_PER_BUFFER;
        memcpy(data, &q->buffer[pos], frames * sizeof(short));
        q->readIndex = (q->readIndex + 1) % 64;
    }
    pthread_mutex_unlock(&q->mutex);
    return !empty;
}

typedef struct
{
    short *buffer;
    size_t size;
    size_t capacity;
    int recording;
    time_t last_sound_time;
    char serial_name[256];
    int amplitude_threshold;
    int debug_amplitude;
    int chunk_size;
} AudioData;

// Global atomic flag to control live listen thread
static atomic_int keep_listening = 1;

static int audioCallback(const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo *timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData)
{
    AudioData *data = (AudioData *)userData;
    const short *input = (const short *)inputBuffer;

    if (!input)
    {
        fprintf(stderr, "No input detected!\n");
        return paContinue;
    }

    // Enqueue audio for live listen
    enqueue(&audioQueue, input, framesPerBuffer);

    int max_amplitude = 0;
    for (unsigned int i = 0; i < framesPerBuffer; i++)
    {
        int sample = abs(input[i]);
        if (sample > max_amplitude)
            max_amplitude = sample;
    }

    if (data->debug_amplitude)
    {
        printf("Max amplitude: %d\n", max_amplitude);
    }

    time_t current_time = time(NULL);

    if (max_amplitude > data->amplitude_threshold && !data->recording)
    {
        printf("Recording started.\n");
        data->recording = 1;
        data->size = 0;
        data->capacity = SAMPLE_RATE * 10; // initial buffer for 10 seconds
        data->buffer = (short *)malloc(data->capacity * sizeof(short));
        if (!data->buffer)
        {
            fprintf(stderr, "Memory allocation failed!\n");
            return paAbort;
        }
        data->last_sound_time = current_time;
    }

    if (data->recording)
    {
        if (data->size + framesPerBuffer > data->capacity)
        {
            data->capacity *= 2;
            data->buffer = realloc(data->buffer, data->capacity * sizeof(short));
            if (!data->buffer)
            {
                fprintf(stderr, "Memory reallocation failed!\n");
                return paAbort;
            }
        }
        memcpy(data->buffer + data->size, input, framesPerBuffer * sizeof(short));
        data->size += framesPerBuffer;

        if (max_amplitude > data->amplitude_threshold)
        {
            data->last_sound_time = current_time;
        }

        if (difftime(current_time, data->last_sound_time) > SILENCE_THRESHOLD)
        {
            printf("Silence detected for too long. Stopping recording...\n");

            size_t remove_samples = REMOVE_LAST_SECONDS * SAMPLE_RATE;
            if (data->size > remove_samples)
            {
                data->size -= remove_samples;
            }
            else
            {
                data->size = 0;
            }

            if (data->size > 0)
            {
                char filename[256], final_file_path[256];
                char time_str[64];
                time_t now = time(NULL);
                struct tm *t = localtime(&now);
                strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", t);
                snprintf(filename, sizeof(filename), "%s_%s.wav", data->serial_name, time_str);
                snprintf(final_file_path, sizeof(final_file_path), RECORDINGS_DIR "/%s", filename);

                if (write_wav_file(final_file_path, data->buffer, data->size, SAMPLE_RATE) == 0)
                {
                    printf("Recording saved: %s\n", final_file_path);
                }
                else
                {
                    fprintf(stderr, "Failed to write WAV file.\n");
                }
            }
            else
            {
                printf("Recording too short, skipping save.\n");
            }

            free(data->buffer);
            data->buffer = NULL;
            data->size = 0;
            data->capacity = 0;
            data->recording = 0;
        }
    }

    return paContinue;
}

// Playback callback for live listening
static int playCallback(const void *inputBuffer, void *outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags,
                        void *userData)
{
    short *out = (short *)outputBuffer;

    if (!dequeue(&audioQueue, out, framesPerBuffer))
    {
        // No data available, output silence
        memset(out, 0, framesPerBuffer * sizeof(short));
    }
    return paContinue;
}

void *liveListenThread(void *arg)
{
    PaStream *playStream;
    PaStreamParameters outputParams;
    PaError err;

    outputParams.device = AUDIO_OUTPUT_DEVICE; // from config.h
    if (outputParams.device == paNoDevice)
    {
        fprintf(stderr, "No output device available for live listen\n");
        return NULL;
    }
    outputParams.channelCount = CHANNELS;
    outputParams.sampleFormat = paInt16;
    outputParams.suggestedLatency = Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;
    outputParams.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(&playStream, NULL, &outputParams, SAMPLE_RATE,
                        FRAMES_PER_BUFFER, paClipOff, playCallback, NULL);
    if (err != paNoError)
    {
        fprintf(stderr, "Failed to open playback stream: %s\n", Pa_GetErrorText(err));
        return NULL;
    }

    err = Pa_StartStream(playStream);
    if (err != paNoError)
    {
        fprintf(stderr, "Failed to start playback stream: %s\n", Pa_GetErrorText(err));
        Pa_CloseStream(playStream);
        return NULL;
    }

    printf("Live listening started...\n");

    // Run until keep_listening is false
    while (atomic_load(&keep_listening))
    {
        Pa_Sleep(100);
    }

    Pa_StopStream(playStream);
    Pa_CloseStream(playStream);
    printf("Live listening stopped.\n");

    return NULL;
}

void recorder(const char *com_port)
{
    PaError err;
    PaStream *recordStream;
    AudioData data = {0};
    pthread_t listenThread;

    if (load_env(".env") != 0)
    {
        printf("Failed to load config\n");
        return;
    }

    data.amplitude_threshold = AMPLITUDE_THRESHOLD;
    data.debug_amplitude = DEBUG_AMPLITUDE;
    data.chunk_size = CHUNK_SIZE;

    char *serial_name = open_serial_port(com_port);
    snprintf(data.serial_name, sizeof(data.serial_name), "%s", serial_name ? serial_name : "unknown");

    err = Pa_Initialize();
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio init error: %s\n", Pa_GetErrorText(err));
        return;
    }

    initQueue(&audioQueue);

    PaStreamParameters inputParams;
    inputParams.device = Pa_GetDefaultInputDevice();
    if (inputParams.device == paNoDevice)
    {
        fprintf(stderr, "No default input device.\n");
        Pa_Terminate();
        return;
    }
    inputParams.channelCount = CHANNELS;
    inputParams.sampleFormat = paInt16;
    inputParams.suggestedLatency = Pa_GetDeviceInfo(inputParams.device)->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(&recordStream, &inputParams, NULL, SAMPLE_RATE,
                        data.chunk_size, paClipOff, audioCallback, &data);
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio stream error: %s\n", Pa_GetErrorText(err));
        Pa_Terminate();
        return;
    }

    err = Pa_StartStream(recordStream);
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio start error: %s\n", Pa_GetErrorText(err));
        Pa_CloseStream(recordStream);
        Pa_Terminate();
        return;
    }

    if (LIVE_LISTEN)
    {
        atomic_store(&keep_listening, 1);
        pthread_create(&listenThread, NULL, liveListenThread, NULL);
    }

    printf("Recorder started. Press Ctrl+C to stop...\n");

    // Simple loop - in real code, catch signals to stop cleanly
    while (1)
    {
        sleep(1);
        // For demo, press Ctrl+C to exit (kill signal)
    }

    // Stop live listen thread if running
    if (LIVE_LISTEN)
    {
        atomic_store(&keep_listening, 0);
        pthread_join(listenThread, NULL);
    }

    Pa_StopStream(recordStream);
    Pa_CloseStream(recordStream);
    Pa_Terminate();

    if (data.buffer)
        free(data.buffer);

    printf("Recorder stopped.\n");
}
