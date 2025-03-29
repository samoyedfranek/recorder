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
#define SILENCE_THRESHOLD 5   // Seconds of silence before stopping
#define REMOVE_LAST_SECONDS 5 // Seconds to remove from the end of the recording

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

    // Compute max amplitude in the chunk
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

    time_t current_time = time(NULL);

    // Start recording if amplitude exceeds threshold
    if (max_amplitude > AMPLITUDE_THRESHOLD && !data->recording)
    {
        printf("Recording started.\n");
        data->recording = 1;
        data->size = 0;
        data->capacity = SAMPLE_RATE * 10; // Allocate for 10 seconds initially
        data->buffer = (short *)malloc(data->capacity * sizeof(short));
        if (!data->buffer)
        {
            fprintf(stderr, "Memory allocation failed!\n");
            return paAbort;
        }
        data->last_sound_time = current_time; // Mark the last detected sound
    }

    // If recording, store all data (including silence)
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
        printf("Buffer size: %zu\n", data->size);

        // Update the last sound detection time if the sound is above the threshold
        if (max_amplitude > AMPLITUDE_THRESHOLD)
        {
            data->last_sound_time = current_time;
        }

        // If no sound is detected, check for silence detection
        if (max_amplitude <= AMPLITUDE_THRESHOLD)
        {
            printf("Silence detected: Max amplitude below threshold\n");
        }

        // Stop recording if silence lasts too long
        if (difftime(current_time, data->last_sound_time) > SILENCE_THRESHOLD)
        {
            printf("Silence detected for too long. Stopping recording...\n");

            // Remove the last 5 seconds from the buffer
            size_t remove_samples = REMOVE_LAST_SECONDS * SAMPLE_RATE;
            if (data->size > remove_samples)
            {
                data->size -= remove_samples;
            }
            else
            {
                // If the recording is shorter than 5 seconds, just reset to 0 size
                data->size = 0;
            }

            // Save the recording
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

            // Reset recording data
            free(data->buffer);
            data->buffer = NULL;
            data->size = 0;
            data->capacity = 0;
            data->recording = 0;
        }
    }

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
