#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <portaudio.h>
#include "h/write_wav_file.h"
#include "h/open_serial_port.h"
#include "h/recordAudio.h"

#define SAMPLE_RATE 48000
#define CHANNELS 1
#define CHUNK_SIZE 1024
#define SILENCE_THRESHOLD 5
#define REMOVE_LAST_SECONDS 5
#define RECORDINGS_DIR "./recordings"

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
    int input_device;
    int output_device;
    int enable_live_listen;
    PaStream *playback_stream;
} AudioData;

void load_env(AudioData *data, const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f)
    {
        fprintf(stderr, "Warning: Could not open %s, using defaults\n", filename);
        data->amplitude_threshold = 300;
        data->debug_amplitude = 0;
        data->input_device = 0;
        data->output_device = -1;
        data->enable_live_listen = 0;
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        if (line[0] == '#' || line[0] == '\n') continue;

        char key[64], value[64];
        if (sscanf(line, "%63[^=]=%63s", key, value) == 2)
        {
            if (strcmp(key, "AMPLITUDE_THRESHOLD") == 0)
                data->amplitude_threshold = atoi(value);
            else if (strcmp(key, "DEBUG_AMPLITUDE") == 0)
                data->debug_amplitude = (strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0) ? 1 : 0;
            else if (strcmp(key, "AUDIO_INPUT_DEVICE") == 0)
                data->input_device = atoi(value);
            else if (strcmp(key, "AUDIO_OUTPUT_DEVICE") == 0)
                data->output_device = atoi(value);
            else if (strcmp(key, "LIVE_LISTEN") == 0)
                data->enable_live_listen = (strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0) ? 1 : 0;
        }
    }

    fclose(f);
}

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

    int max_amplitude = 0;
    for (unsigned int i = 0; i < framesPerBuffer; i++)
    {
        int sample = abs(input[i]);
        if (sample > max_amplitude)
            max_amplitude = sample;
    }

    if (data->debug_amplitude)
        printf("Max amplitude: %d\n", max_amplitude);

    time_t current_time = time(NULL);

    if (max_amplitude > data->amplitude_threshold && !data->recording)
    {
        printf("Recording started.\n");
        data->recording = 1;
        data->size = 0;
        data->capacity = SAMPLE_RATE * 10;
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
            data->last_sound_time = current_time;

        if (difftime(current_time, data->last_sound_time) > SILENCE_THRESHOLD)
        {
            printf("Silence detected. Stopping recording...\n");

            size_t remove_samples = REMOVE_LAST_SECONDS * SAMPLE_RATE;
            if (data->size > remove_samples)
                data->size -= remove_samples;
            else
                data->size = 0;

            if (data->size > 0)
            {
                char filename[256], final_file_path[256], time_str[64];
                time_t now = time(NULL);
                struct tm *tm_info = localtime(&now);
                strftime(time_str, sizeof(time_str), "%Y-%m-%d_%H-%M-%S", tm_info);
                snprintf(filename, sizeof(filename), "%s.wav", time_str);
                snprintf(final_file_path, sizeof(final_file_path), "%s/%s", RECORDINGS_DIR, filename);
                write_wav_file(final_file_path, data->buffer, data->size, SAMPLE_RATE);
                printf("Saved: %s\n", final_file_path);
            }

            free(data->buffer);
            data->buffer = NULL;
            data->recording = 0;
        }
    }

    // Live listen
    if (data->enable_live_listen && data->playback_stream)
    {
        Pa_WriteStream(data->playback_stream, input, framesPerBuffer);
    }

    return paContinue;
}

int main(void)
{
    AudioData data = {0};
    load_env(&data, ".env");

    mkdir(RECORDINGS_DIR, 0777);

    Pa_Initialize();

    PaStream *stream;
    PaStreamParameters inputParams;
    inputParams.device = data.input_device;
    inputParams.channelCount = CHANNELS;
    inputParams.sampleFormat = paInt16;
    inputParams.suggestedLatency = Pa_GetDeviceInfo(inputParams.device)->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = NULL;

    PaStreamParameters outputParams;
    if (data.enable_live_listen)
    {
        outputParams.device = data.output_device;
        outputParams.channelCount = CHANNELS;
        outputParams.sampleFormat = paInt16;
        outputParams.suggestedLatency = Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;
        outputParams.hostApiSpecificStreamInfo = NULL;

        Pa_OpenStream(&data.playback_stream, NULL, &outputParams,
                      SAMPLE_RATE, CHUNK_SIZE, paNoFlag, NULL, NULL);
        Pa_StartStream(data.playback_stream);
    }

    Pa_OpenStream(&stream, &inputParams, NULL,
                  SAMPLE_RATE, CHUNK_SIZE, paClipOff,
                  audioCallback, &data);
    Pa_StartStream(stream);

    printf("Recording started. Press Ctrl+C to stop.\n");
    while (1)
    {
        sleep(1);
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);

    if (data.enable_live_listen && data.playback_stream)
    {
        Pa_StopStream(data.playback_stream);
        Pa_CloseStream(data.playback_stream);
    }

    Pa_Terminate();
    return 0;
}
