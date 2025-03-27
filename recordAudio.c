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

// --- Configuration constants ---
#define RATE 48000
#define CHUNK_SIZE 2048
#define AMPLITUDE_THRESHOLD 300 // Amplitude threshold to consider as silence
#define SILENCE_THRESHOLD 5     // Seconds of silence before stopping recording

// Directories for saving files
#define CACHE_DIR "./cache"
#define RECORDINGS_DIR "./recordings"

// --- Forward declaration of external function ---
extern char *open_serial_port(const char *com_port);

// --- Helper: Move file (rename) from cache to recordings ---
void move_file_to_recordings(const char *temp_file_path, const char *final_file_path)
{
    if (rename(temp_file_path, final_file_path) == 0)
    {
        printf("File moved to: %s\n", final_file_path);
    }
    else
    {
        perror("Error moving file");
    }
}

// --- Helper: Write WAV file ---
// Writes a minimal WAV header and raw PCM data (16-bit mono) to file.
int write_wav_file(const char *filename, short *data, size_t num_samples, int sample_rate)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp)
    {
        perror("fopen");
        return -1;
    }
    int byte_rate = sample_rate * sizeof(short);
    int block_align = sizeof(short);
    int bits_per_sample = 16;
    int subchunk2_size = num_samples * sizeof(short);
    int chunk_size = 36 + subchunk2_size;

    // Write header
    fwrite("RIFF", 1, 4, fp);
    fwrite(&chunk_size, 4, 1, fp);
    fwrite("WAVE", 1, 4, fp);

    // fmt subchunk
    fwrite("fmt ", 1, 4, fp);
    int subchunk1_size = 16;
    fwrite(&subchunk1_size, 4, 1, fp);
    short audio_format = 1; // PCM format
    fwrite(&audio_format, 2, 1, fp);
    short num_channels = 1;
    fwrite(&num_channels, 2, 1, fp);
    fwrite(&sample_rate, 4, 1, fp);
    fwrite(&byte_rate, 4, 1, fp);
    fwrite(&block_align, 2, 1, fp);
    fwrite(&bits_per_sample, 2, 1, fp);

    // data subchunk
    fwrite("data", 1, 4, fp);
    fwrite(&subchunk2_size, 4, 1, fp);
    fwrite(data, sizeof(short), num_samples, fp);

    fclose(fp);
    return 0;
}

// --- Main recording function ---
void recorder(int input_device_id, const char *com_port)
{
    PaError err;
    err = Pa_Initialize();
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
        return;
    }

    // Validate input device ID
    int device_count = Pa_GetDeviceCount();
    if (input_device_id < 0 || input_device_id >= device_count)
    {
        fprintf(stderr, "Invalid input device ID: %d\n", input_device_id);
        Pa_Terminate();
        return;
    }

    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(input_device_id);
    if (deviceInfo == NULL)
    {
        fprintf(stderr, "Failed to get device info for device ID: %d\n", input_device_id);
        Pa_Terminate();
        return;
    }

    // Log the device name
    printf("Using input device: %s (ID: %d)\n", deviceInfo->name, input_device_id);

    // Define the input parameters
    PaStreamParameters inputParameters;
    memset(&inputParameters, 0, sizeof(inputParameters));
    inputParameters.device = input_device_id;
    inputParameters.channelCount = 1;       // Mono input
    inputParameters.sampleFormat = paInt16; // 16-bit integer format
    inputParameters.suggestedLatency = deviceInfo->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    PaStream *stream;
    err = Pa_OpenStream(&stream,
                        &inputParameters,
                        NULL, // No output parameters
                        RATE,
                        CHUNK_SIZE,
                        paClipOff, // No clipping
                        NULL,
                        NULL);
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

    // Get a serial name from the COM port via external function.
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

        // Compute maximum amplitude in the chunk
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
            // Audio detected above threshold
            if (!recording)
            {
                printf("Recording started.\n");
                recording = 1;
                last_sound_time = current_time;
                // Allocate initial buffer for ~10 seconds
                audio_buffer_capacity = RATE * 10;
                audio_buffer_size = 0;
                audio_buffer = (short *)malloc(audio_buffer_capacity * sizeof(short));
                if (!audio_buffer)
                {
                    fprintf(stderr, "Memory allocation failed\n");
                    break;
                }
            }

            // Ensure capacity and append chunk
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
            last_sound_time = current_time; // Update only when sound is detected
        }
        else if (recording)
        {
            // Silence detected below threshold: append silence to the buffer
            // Use zero for silence in the audio buffer
            short silence[CHUNK_SIZE] = {0}; // A chunk of silence (all zeros)

            // Ensure capacity and append silence
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

            // Append silence to the buffer
            memcpy(audio_buffer + audio_buffer_size, silence, CHUNK_SIZE * sizeof(short));
            audio_buffer_size += CHUNK_SIZE;

            // Check for silence duration
            // Check for silence duration
            if (difftime(current_time, last_sound_time) > SILENCE_THRESHOLD)
            {
                printf("Silence detected. Saving recording...\n");

                // Cut 5 seconds from the end of the recording
                size_t cut_samples = RATE * 5; // 5 seconds of samples
                if (audio_buffer_size > cut_samples)
                {
                    audio_buffer_size -= cut_samples; // Reduce buffer size by 5 seconds
                }
                else
                {
                    audio_buffer_size = 0; // If the recording is shorter than 5 seconds, set to 0
                }

                size_t final_samples = audio_buffer_size;

                // Build file names using serial_name and current timestamp
                char filename[256], temp_file_path[256], final_file_path[256];
                char time_str[64];
                time_t now = time(NULL);
                struct tm *t = localtime(&now);
                strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", t);
                snprintf(filename, sizeof(filename), "%s_%s.wav", serial_name, time_str);
                snprintf(temp_file_path, sizeof(temp_file_path), CACHE_DIR "/%s", filename);
                snprintf(final_file_path, sizeof(final_file_path), RECORDINGS_DIR "/%s", filename);

                if (final_samples > 0)
                {
                    if (write_wav_file(temp_file_path, audio_buffer, final_samples, RATE) == 0)
                    {
                        move_file_to_recordings(temp_file_path, final_file_path);
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
        else
        {
            // If not recording, continue reading and checking for silence
        }

        // Sleep briefly to reduce CPU usage (10ms)
        usleep(10000);
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
}
