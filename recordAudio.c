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
#include "h/config.h"

#define SAMPLE_RATE 48000
#define CHANNELS 1
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
    int chunk_size;
} AudioData;

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
    {
        printf("Max amplitude: %d\n", max_amplitude);
    }

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

void recorder(const char *com_port)
{
    PaError err;
    PaStream *stream;
    AudioData data = {0};

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

    PaStreamParameters inputParams;
    if (AUDIO_INPUT_DEVICE < 0 || AUDIO_INPUT_DEVICE >= Pa_GetDeviceCount())
    {
        fprintf(stderr, "Invalid AUDIO_INPUT_DEVICE index (%d). Falling back to default.\n", AUDIO_INPUT_DEVICE);
        inputParams.device = Pa_GetDefaultInputDevice();
    }
    else
    {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(AUDIO_INPUT_DEVICE);
        if (info == NULL || info->maxInputChannels < CHANNELS)
        {
            fprintf(stderr, "Specified AUDIO_INPUT_DEVICE (%d) not suitable (null or too few input channels). Falling back to default.\n", AUDIO_INPUT_DEVICE);
            inputParams.device = Pa_GetDefaultInputDevice();
        }
        else
        {
            inputParams.device = AUDIO_INPUT_DEVICE;
        }
    }

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

    err = Pa_OpenStream(&stream,
                        &inputParams,
                        NULL,
                        SAMPLE_RATE,
                        data.chunk_size,
                        paClipOff,
                        audioCallback,
                        &data);
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio stream error: %s\n", Pa_GetErrorText(err));
        Pa_Terminate();
        return;
    }

    err = Pa_StartStream(stream);
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio start error: %s\n", Pa_GetErrorText(err));
        Pa_CloseStream(stream);
        Pa_Terminate();
        return;
    }

    printf("Started recording on serial: %s\n", data.serial_name);
    if (data.debug_amplitude)
        printf("Amplitude debugging enabled. Threshold: %d\n", data.amplitude_threshold);

    while (1)
    {
        sleep(1);
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
}
