#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <portaudio.h>
#include "h/open_serial_port.h"
#include "h/write_wav_file.h"

#define SAMPLE_RATE 48000
#define CHANNELS 1
#define SAMPLE_SIZE 2   // 16-bit samples (2 bytes per sample)
#define CHUNK_SIZE 1024 // Frames per read
#define AMPLITUDE_THRESHOLD 300
#define SILENCE_THRESHOLD 5 // Seconds of silence before stopping

typedef struct
{
    short *buffer;
    size_t size;
    size_t capacity;
    int recording;
    time_t last_sound_time;
} AudioData;

static int audioCallback(const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo *timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData)
{
    (void)outputBuffer;
    (void)timeInfo;
    (void)statusFlags;

    AudioData *data = (AudioData *)userData;

    if (inputBuffer == NULL)
    {
        fprintf(stderr, "No input detected!\n");
        return paContinue;
    }

    // Cast input buffer to short (16-bit PCM)
    const short *samples = (const short *)inputBuffer;

    // Compute max amplitude
    int max_amplitude = 0;
    for (unsigned int i = 0; i < framesPerBuffer; i++)
    {
        int sample = abs(samples[i]);
        if (sample > max_amplitude)
        {
            max_amplitude = sample;
        }
    }

    // Debugging: Print first few sample values
    printf("Sample[0] = %d, Sample[1] = %d, Max Amplitude: %d\n", samples[0], samples[1], max_amplitude);

    time_t current_time = time(NULL);
    if (max_amplitude > AMPLITUDE_THRESHOLD)
    {
        if (!data->recording)
        {
            printf("Recording started.\n");
            data->recording = 1;
            data->last_sound_time = current_time;
            data->capacity = SAMPLE_RATE * 10; // Allocate space for 10 seconds
            data->size = 0;
            data->buffer = (short *)realloc(data->buffer, data->capacity * sizeof(short));
            if (!data->buffer)
            {
                fprintf(stderr, "Memory allocation failed\n");
                return paAbort;
            }
        }

        // Ensure buffer space
        if (data->size + framesPerBuffer > data->capacity)
        {
            data->capacity *= 2;
            data->buffer = realloc(data->buffer, data->capacity * sizeof(short));
            if (!data->buffer)
            {
                fprintf(stderr, "Memory reallocation failed\n");
                return paAbort;
            }
        }

        // Copy audio data into buffer
        memcpy(data->buffer + data->size, samples, framesPerBuffer * sizeof(short));
        data->size += framesPerBuffer;
        data->last_sound_time = current_time;
    }
    else if (data->recording)
    {
        // Add silence if still recording
        short silence[CHUNK_SIZE] = {0};
        if (data->size + framesPerBuffer > data->capacity)
        {
            data->capacity *= 2;
            data->buffer = realloc(data->buffer, data->capacity * sizeof(short));
            if (!data->buffer)
            {
                fprintf(stderr, "Memory reallocation failed\n");
                return paAbort;
            }
        }
        memcpy(data->buffer + data->size, silence, framesPerBuffer * sizeof(short));
        data->size += framesPerBuffer;

        // Check for silence duration
        if (difftime(current_time, data->last_sound_time) > SILENCE_THRESHOLD)
        {
            printf("Silence detected. Saving recording...\n");

            // Trim last 4 seconds of silence
            size_t cut_samples = SAMPLE_RATE * 4;
            if (data->size > cut_samples)
            {
                data->size -= cut_samples;
            }
            else
            {
                data->size = 0;
            }

            // Save recording if long enough
            if (data->size > 0)
            {
                printf("Recording saved (size: %zu samples)\n", data->size);
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

void startRecording()
{
    PaError err;
    PaStream *stream;
    AudioData data = {NULL, 0, 0, 0, 0};

    // Initialize PortAudio
    err = Pa_Initialize();
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
        return;
    }

    // Open stream
    err = Pa_OpenDefaultStream(&stream, CHANNELS, 0, paInt16, SAMPLE_RATE,
                               CHUNK_SIZE, audioCallback, &data);
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
        Pa_Terminate();
        return;
    }

    // Start stream
    err = Pa_StartStream(stream);
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
        Pa_CloseStream(stream);
        Pa_Terminate();
        return;
    }

    printf("Recording loop started...\nPress Enter to stop.\n");
    getchar(); // Wait for user input

    // Stop stream
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    printf("Recording stopped.\n");
}