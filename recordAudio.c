#include "h/recordAudio.h"
#include "h/write_wav_file.h"
#include "h/open_serial_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <portaudio.h>
#include <sys/stat.h>
#include <sys/types.h>

// --- Configuration Constants ---
#define RATE 48000
#define CHUNK_SIZE 1024 // Reduced from 2048 to prevent buffer overflow
#define AMPLITUDE_THRESHOLD 300
#define SILENCE_THRESHOLD 5 // Seconds of silence before stopping

// Directories for saving files
#define CACHE_DIR "./cache"
#define RECORDINGS_DIR "./recordings"

// --- Forward declaration ---
extern char *open_serial_port(const char *com_port);

// --- Recorder Function ---
void recorder(int input_device_id, const char *com_port)
{
    PaError err = Pa_Initialize();
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
        return;
    }

    PaStream *stream;
    err = Pa_OpenDefaultStream(&stream, 1, 0, paInt16, RATE, CHUNK_SIZE, NULL, NULL);
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
        Pa_Terminate();
        return;
    }

    err = Pa_StartStream(stream);
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
        Pa_CloseStream(stream);
        Pa_Terminate();
        return;
    }

    time_t last_log_time = time(NULL);

    printf("Starting recording loop...\n");
    while (1)
    {
        short chunk[CHUNK_SIZE];
        err = Pa_ReadStream(stream, chunk, CHUNK_SIZE);
        if (err != paNoError)
        {
            fprintf(stderr, "PortAudio read error: %s\n", Pa_GetErrorText(err));
            usleep(100000); // Sleep 100ms on error
            continue;
        }

        // Compute max amplitude in chunk
        int max_amplitude = 0;
        for (int i = 0; i < CHUNK_SIZE; i++)
        {
            int sample = abs(chunk[i]);
            if (sample > max_amplitude)
                max_amplitude = sample;
        }

        time_t current_time = time(NULL);
        if (difftime(current_time, last_log_time) >= 1)
        {
            printf("Current Amplitude: %d (Threshold: %d)\n", max_amplitude, AMPLITUDE_THRESHOLD);
            last_log_time = current_time;
        }

        usleep(10000); // Reduce CPU usage
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
}
