#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <portaudio.h>
#include "h/write_wav_file.h"
#include "h/open_serial_port.h"
#include "h/recordAudio.h"
#include "h/config.h"

#define SAMPLE_RATE 48000
#define CHANNELS 1
#define PREBUFFER_SECONDS 1
#define PREBUFFER_SIZE (SAMPLE_RATE * PREBUFFER_SECONDS)
#define RECORDING_CHECK_INTERVAL 20

typedef struct
{
    short *buffer;
    size_t size;
    size_t capacity;
    int recording;
    int recording_check_counter;
    int recording_total_chunks;
    time_t last_sound_time;
    char serial_name[256];
    int amplitude_threshold;
    int chunk_size;
    short prebuffer[PREBUFFER_SIZE];
    size_t prebuffer_index;
    int prebuffer_full;
    int live_listen; 
} AudioData;

static int audioCallback(const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo *timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData)
{
    AudioData *data = (AudioData *)userData;
    const short *input = (const short *)inputBuffer;
    short *output = (short *)outputBuffer;

    // --- LIVE LISTEN (Pass-through) LOGIC ---
    if (output)
    {
        if (data->live_listen && input)
        {
            // COPY INPUT TO OUTPUT
            // OPTIONAL: You can multiply samples here to boost volume 
            // e.g., output[i] = input[i] * 2; (Requires loop instead of memcpy)
            memcpy(output, input, framesPerBuffer * sizeof(short));
        }
        else
        {
            // Mute output if disabled or no input to prevent static/buzzing
            memset(output, 0, framesPerBuffer * sizeof(short));
        }
    }
    // ----------------------------------------

    if (!input)
    {
        // Do not spam stderr, just return
        return paContinue;
    }
    for (unsigned int i = 0; i < framesPerBuffer; i++)
    {
        data->prebuffer[data->prebuffer_index] = input[i];
        data->prebuffer_index = (data->prebuffer_index + 1) % PREBUFFER_SIZE;
    }
    if (data->prebuffer_index == 0)
        data->prebuffer_full = 1;

    int max_amplitude = 0;
    for (unsigned int i = 0; i < framesPerBuffer; i++)
    {
        int sample = abs(input[i]);
        if (sample > max_amplitude)
            max_amplitude = sample;
    }
    time_t current_time = time(NULL);

    if (max_amplitude > data->amplitude_threshold && !data->recording)
    {
        printf("Recording started.\n");
        data->recording = 1;
        data->recording_check_counter = 0;
        data->size = 0;
        data->capacity = SAMPLE_RATE * 10;
        data->buffer = (short *)malloc(data->capacity * sizeof(short));
        if (!data->buffer)
        {
            fprintf(stderr, "Memory allocation failed!\n");
            return paAbort;
        }

        size_t pre_count = data->prebuffer_full ? PREBUFFER_SIZE : data->prebuffer_index;
        size_t start_index = data->prebuffer_index; // Most recent sample

        int start_offset = -1;
        for (size_t i = 0; i < pre_count; i++)
        {
            size_t idx = (start_index + i) % PREBUFFER_SIZE;
            if (abs(data->prebuffer[idx]) > data->amplitude_threshold / 2)
            {
                start_offset = i;
                break;
            }
        }

        if (start_offset != -1)
        {
            for (size_t i = start_offset; i < pre_count; i++)
            {
                size_t idx = (start_index + i) % PREBUFFER_SIZE;
                data->buffer[data->size++] = data->prebuffer[idx];
            }
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
        data->recording_total_chunks++;

        data->recording_check_counter++;
        if (data->recording_check_counter >= RECORDING_CHECK_INTERVAL)
        {
            data->recording_check_counter = 0;

            double recording_time_sec = (double)data->size / SAMPLE_RATE;

            time_t raw_time = time(NULL);
            struct tm *time_info = localtime(&raw_time);
            char datetime_str[64];
            strftime(datetime_str, sizeof(datetime_str), "%Y-%m-%d %H:%M:%S", time_info);

            struct tm *last_tm = localtime(&data->last_sound_time);
            char last_sound_str[32];
            strftime(last_sound_str, sizeof(last_sound_str), "%H:%M:%S", last_tm);

            double silence_duration = difftime(raw_time, data->last_sound_time);
            printf("[RECORDING] DateTime: %s | Last sound: %s | Silence: %.2fs | Max Amplitude: %d | Chunks: %d | Samples: %zu | Recording time: %.2fs\n",
                   datetime_str,
                   last_sound_str,
                   silence_duration,
                   max_amplitude,
                   data->recording_total_chunks,
                   data->size,
                   recording_time_sec);
        }

        if (max_amplitude > data->amplitude_threshold)
        {
            data->last_sound_time = current_time;
        }

        if (difftime(current_time, data->last_sound_time) > SILENCE_THRESHOLD)
        {
            printf("Silence detected. Stopping recording...\n");

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
                char filename[256], final_file_path[256], time_str[64];
                time_t now = time(NULL);
                struct tm *t = localtime(&now);
                strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", t);
                snprintf(filename, sizeof(filename), "%s_%s.wav", data->serial_name, time_str);
                snprintf(final_file_path, sizeof(final_file_path), "%s/%s", RECORDING_DIRECTORY, filename);

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

int findInputDeviceByName(const char *name)
{
    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0)
    {
        fprintf(stderr, "Pa_GetDeviceCount returned %d\n", numDevices);
        return paNoDevice;
    }

    for (int i = 0; i < numDevices; i++)
    {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        if (!info)
            continue;

        if (info->maxInputChannels > 0 && strstr(info->name, name) != NULL)
        {
            return i;
        }
    }

    return paNoDevice;
}void recorder(const char *com_port)
{
    PaError err;
    PaStream *stream;
    AudioData data = {0};

    if (load_env(".env") != 0) printf("Failed to load config (using defaults)\n");

    data.amplitude_threshold = AMPLITUDE_THRESHOLD;
    data.chunk_size = CHUNK_SIZE;
    data.recording_total_chunks = 0;

    // --- FORCE LIVE LISTEN ON ---
    data.live_listen = 1; 
    // ---------------------------

    const char *AUDIO_DEVICE_NAME = "hw:0,0"; 
    char *serial_name = "radio"; 
    snprintf(data.serial_name, sizeof(data.serial_name), "%s", serial_name ? serial_name : "unknown");

    err = Pa_Initialize();
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio init error: %s\n", Pa_GetErrorText(err));
        return;
    }

    // 1. Setup Input (Microphone)
    PaStreamParameters inputParams;
    int inputDeviceIndex = findInputDeviceByName(AUDIO_DEVICE_NAME);

    if (inputDeviceIndex == paNoDevice)
    {
        fprintf(stderr, "Error: Input device '%s' not found.\n", AUDIO_DEVICE_NAME);
        Pa_Terminate();
        return;
    }
    inputParams.device = inputDeviceIndex;
    inputParams.channelCount = CHANNELS;
    inputParams.sampleFormat = paInt16;
    inputParams.suggestedLatency = Pa_GetDeviceInfo(inputParams.device)->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = NULL;

    printf("Input Device: %s\n", Pa_GetDeviceInfo(inputDeviceIndex)->name);

    // 2. Setup Output (Speakers)
    PaStreamParameters outputParams;
    PaStreamParameters *pOutputParams = NULL;

    if (data.live_listen)
    {
        int outputDeviceIndex = Pa_GetDefaultOutputDevice();
        if (outputDeviceIndex == paNoDevice)
        {
            fprintf(stderr, "Warning: No default output device found. Live listen disabled.\n");
            data.live_listen = 0;
        }
        else
        {
            outputParams.device = outputDeviceIndex;
            outputParams.channelCount = CHANNELS;
            outputParams.sampleFormat = paInt16;
            outputParams.suggestedLatency = Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;
            outputParams.hostApiSpecificStreamInfo = NULL;
            pOutputParams = &outputParams;
            
            printf("Output Device (Live Listen): %s\n", Pa_GetDeviceInfo(outputDeviceIndex)->name);
        }
    }

    // 3. Open Stream (Duplex)
    // We use the output parameters here. If pOutputParams is NULL, it opens strictly for recording.
    err = Pa_OpenStream(&stream, 
                        &inputParams, 
                        pOutputParams, // Passing this enables the output channel
                        SAMPLE_RATE, 
                        data.chunk_size, 
                        paClipOff, 
                        audioCallback, 
                        &data);

    if (err != paNoError)
    {
        fprintf(stderr, "Stream error: %s\n", Pa_GetErrorText(err));
        Pa_Terminate();
        return;
    }

    err = Pa_StartStream(stream);
    if (err != paNoError)
    {
        fprintf(stderr, "Start error: %s\n", Pa_GetErrorText(err));
        Pa_CloseStream(stream);
        Pa_Terminate();
        return;
    }

    printf("Recorder running... (Press Ctrl+C to stop)\n");

    while (1)
    {
        sleep(1);
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
}