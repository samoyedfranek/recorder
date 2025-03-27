#include "h/recordAudio.h"
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
    PaError err;
    err = Pa_Initialize();
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
        return;
    }

    // Print selected input device name
    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(input_device_id);
    if (deviceInfo)
    {
        printf("Using input device: ID %d - %s\n", input_device_id, deviceInfo->name);
    }
    else
    {
        printf("Invalid device ID!\n");
        Pa_Terminate();
        return;
    }

    // Define input parameters
    PaStreamParameters inputParameters;
    inputParameters.device = input_device_id;
    inputParameters.channelCount = 1;
    inputParameters.sampleFormat = paInt16;
    inputParameters.suggestedLatency = deviceInfo->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    // Open the audio stream
    PaStream *stream;
    err = Pa_OpenStream(&stream, &inputParameters, NULL, RATE, CHUNK_SIZE, paClipOff, NULL, NULL);
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

    // Open serial port for filename
    char *serial_name = open_serial_port(com_port);
    if (!serial_name)
    {
        serial_name = "unknown";
    }

    int recording = 0;
    time_t last_sound_time = 0;
    short *audio_buffer = NULL;
    size_t audio_buffer_size = 0;
    size_t audio_buffer_capacity = 0;

    printf("Recording loop started...\n");
    while (1)
    {
        short chunk[CHUNK_SIZE];
        err = Pa_ReadStream(stream, chunk, CHUNK_SIZE);

        if (err == paInputOverflowed)
        {
            fprintf(stderr, "Warning: Input overflow occurred, skipping chunk.\n");
            continue; // Skip this chunk instead of stopping everything
        }
        else if (err != paNoError)
        {
            fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
            break;
        }

        // Compute maximum amplitude
        int max_amplitude = 0;
        for (int i = 0; i < CHUNK_SIZE; i++)
        {
            int sample = abs(chunk[i]);
            if (sample > max_amplitude)
                max_amplitude = sample;
        }

        time_t current_time = time(NULL);
        if (max_amplitude > AMPLITUDE_THRESHOLD)
        {
            if (!recording)
            {
                printf("Recording started.\n");
                recording = 1;
                last_sound_time = current_time;
                audio_buffer_capacity = RATE * 10;
                audio_buffer_size = 0;
                audio_buffer = (short *)malloc(audio_buffer_capacity * sizeof(short));
                if (!audio_buffer)
                {
                    fprintf(stderr, "Memory allocation failed\n");
                    break;
                }
            }

            if (audio_buffer_size + CHUNK_SIZE > audio_buffer_capacity)
            {
                audio_buffer_capacity *= 2;
                audio_buffer = realloc(audio_buffer, audio_buffer_capacity * sizeof(short));
                if (!audio_buffer)
                {
                    fprintf(stderr, "Memory reallocation failed\n");
                    break;
                }
            }
            memcpy(audio_buffer + audio_buffer_size, chunk, CHUNK_SIZE * sizeof(short));
            audio_buffer_size += CHUNK_SIZE;
            last_sound_time = current_time;
        }
        else if (recording)
        {
            short silence[CHUNK_SIZE] = {0};

            if (audio_buffer_size + CHUNK_SIZE > audio_buffer_capacity)
            {
                audio_buffer_capacity *= 2;
                audio_buffer = realloc(audio_buffer, audio_buffer_capacity * sizeof(short));
                if (!audio_buffer)
                {
                    fprintf(stderr, "Memory reallocation failed\n");
                    break;
                }
            }
            memcpy(audio_buffer + audio_buffer_size, silence, CHUNK_SIZE * sizeof(short));
            audio_buffer_size += CHUNK_SIZE;

            if (difftime(current_time, last_sound_time) > SILENCE_THRESHOLD)
            {
                printf("Silence detected. Saving recording...\n");

                size_t cut_samples = RATE * 4;
                if (audio_buffer_size > cut_samples)
                {
                    audio_buffer_size -= cut_samples;
                }
                else
                {
                    audio_buffer_size = 0;
                }

                if (audio_buffer_size > 0)
                {
                    char filename[256], final_file_path[256];
                    char time_str[64];
                    time_t now = time(NULL);
                    struct tm *t = localtime(&now);
                    strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", t);
                    snprintf(filename, sizeof(filename), "%s_%s.wav", serial_name, time_str);
                    snprintf(final_file_path, sizeof(final_file_path), RECORDINGS_DIR "/%s", filename);

                    if (write_wav_file(final_file_path, audio_buffer, audio_buffer_size, RATE) == 0)
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

                free(audio_buffer);
                audio_buffer = NULL;
                audio_buffer_size = 0;
                audio_buffer_capacity = 0;
                recording = 0;
            }
        }

        usleep(10000);
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
}
