#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <portaudio.h>
#include "h/open_serial_port.h"
#include "h/write_wav_file.h"

// --- Configuration Constants ---
#define SAMPLE_RATE 48000
#define CHANNELS 1
#define CHUNK_SIZE 1024 // Frames per buffer
#define AMPLITUDE_THRESHOLD 300
#define SILENCE_THRESHOLD 5 // Seconds of silence before stopping

// Directories for saving files
#define CACHE_DIR "./cache"
#define RECORDINGS_DIR "./recordings"

// --- Audio Data Structure ---
typedef struct
{
    short *buffer;
    size_t size;
    size_t capacity;
    int recording;
    time_t last_sound_time;
    char serial_name[256];
} AudioData;

// --- Audio Callback Function ---
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

    // Compute maximum amplitude in the chunk
    int max_amplitude = 0;
    for (unsigned int i = 0; i < framesPerBuffer; i++)
    {
        int sample = abs(input[i]);
        if (sample > max_amplitude)
        {
            max_amplitude = sample;
        }
    }
    printf("Frames captured: %lu, Max amplitude: %d\n", framesPerBuffer, max_amplitude);

    // Ensure buffer allocation before recording
    if (!data->buffer)
    {
        data->capacity = SAMPLE_RATE * 60; // Allocate space for at least 1 minute
        data->size = 0;
        data->buffer = (short *)malloc(data->capacity * sizeof(short));
        if (!data->buffer)
        {
            fprintf(stderr, "Memory allocation failed!\n");
            return paAbort;
        }
    }

    // Expand buffer if needed
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

    // Store audio data regardless of silence
    memcpy(data->buffer + data->size, input, framesPerBuffer * sizeof(short));
    data->size += framesPerBuffer;
    printf("Buffer size: %zu\n", data->size);

    return paContinue;
}

// --- Recorder Function ---
void recorder(const char *com_port)
{
    PaError err;
    PaStream *stream;
    AudioData data = {0};

    // Get serial name for filename
    char *serial_name = open_serial_port(com_port);
    snprintf(data.serial_name, sizeof(data.serial_name), "%s", serial_name ? serial_name : "unknown");

    // Initialize PortAudio
    err = Pa_Initialize();
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio init error: %s\n", Pa_GetErrorText(err));
        return;
    }

    // Open stream
    err = Pa_OpenDefaultStream(&stream, CHANNELS, 0, paInt16, SAMPLE_RATE,
                               CHUNK_SIZE, audioCallback, &data);
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio stream error: %s\n", Pa_GetErrorText(err));
        Pa_Terminate();
        return;
    }

    // Start recording
    err = Pa_StartStream(stream);
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio start error: %s\n", Pa_GetErrorText(err));
        Pa_CloseStream(stream);
        Pa_Terminate();
        return;
    }

    printf("Recording loop started... Press Enter to stop.\n");
    getchar();

    // Stop recording
    err = Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    // Free buffer if needed
    if (data.buffer)
    {
        free(data.buffer);
    }
}
