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

// --- SETTINGS ---
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
    int live_listen_active; // Tracks if speaker output is actually working
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

    // 1. INPUT CHECK
    if (input == NULL)
    {
        // If we have no input, mute output and return
        if (output) memset(output, 0, framesPerBuffer * sizeof(short));
        return paContinue;
    }

    // 2. LIVE LISTEN (Speaker Output)
    if (output != NULL)
    {
        if (data->live_listen_active)
        {
            // Copy Mic -> Speaker
            memcpy(output, input, framesPerBuffer * sizeof(short));
        }
        else
        {
            memset(output, 0, framesPerBuffer * sizeof(short));
        }
    }

    // --- RECORDING LOGIC (Unchanged) ---
    // Update Pre-buffer
    for (unsigned int i = 0; i < framesPerBuffer; i++)
    {
        data->prebuffer[data->prebuffer_index] = input[i];
        data->prebuffer_index = (data->prebuffer_index + 1) % PREBUFFER_SIZE;
    }
    if (data->prebuffer_index == 0) data->prebuffer_full = 1;

    // Check Amplitude
    int max_amplitude = 0;
    for (unsigned int i = 0; i < framesPerBuffer; i++)
    {
        int sample = abs(input[i]);
        if (sample > max_amplitude) max_amplitude = sample;
    }
    time_t current_time = time(NULL);

    // Start Recording?
    if (max_amplitude > data->amplitude_threshold && !data->recording)
    {
        printf("Recording STARTED (Amp: %d)\n", max_amplitude);
        data->recording = 1;
        data->recording_check_counter = 0;
        data->size = 0;
        data->capacity = SAMPLE_RATE * 10;
        data->buffer = (short *)malloc(data->capacity * sizeof(short));
        if (!data->buffer) return paAbort;

        // Dump Prebuffer
        size_t pre_count = data->prebuffer_full ? PREBUFFER_SIZE : data->prebuffer_index;
        size_t start_index = data->prebuffer_index; 
        int start_offset = -1;
        for (size_t i = 0; i < pre_count; i++) {
            size_t idx = (start_index + i) % PREBUFFER_SIZE;
            if (abs(data->prebuffer[idx]) > data->amplitude_threshold / 2) {
                start_offset = i; break;
            }
        }
        if (start_offset != -1) {
            for (size_t i = start_offset; i < pre_count; i++) {
                size_t idx = (start_index + i) % PREBUFFER_SIZE;
                data->buffer[data->size++] = data->prebuffer[idx];
            }
        }
        data->last_sound_time = current_time;
    }

    // Continue Recording
    if (data->recording)
    {
        if (data->size + framesPerBuffer > data->capacity) {
            data->capacity *= 2;
            data->buffer = realloc(data->buffer, data->capacity * sizeof(short));
            if (!data->buffer) return paAbort;
        }

        memcpy(data->buffer + data->size, input, framesPerBuffer * sizeof(short));
        data->size += framesPerBuffer;
        data->recording_total_chunks++;

        data->recording_check_counter++;
        if (data->recording_check_counter >= RECORDING_CHECK_INTERVAL) {
            data->recording_check_counter = 0;
            // Debug print to prove callback is running
            // printf("."); fflush(stdout); 
        }

        if (max_amplitude > data->amplitude_threshold) {
            data->last_sound_time = current_time;
        }

        if (difftime(current_time, data->last_sound_time) > SILENCE_THRESHOLD)
        {
            printf("\nSilence detected. Saving...\n");
            
            size_t remove_samples = REMOVE_LAST_SECONDS * SAMPLE_RATE;
            if (data->size > remove_samples) data->size -= remove_samples;
            else data->size = 0;

            if (data->size > 0) {
                char filename[256], final_file_path[256], time_str[64];
                time_t now = time(NULL);
                struct tm *t = localtime(&now);
                strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", t);
                snprintf(filename, sizeof(filename), "%s_%s.wav", data->serial_name, time_str);
                snprintf(final_file_path, sizeof(final_file_path), "%s/%s", RECORDING_DIRECTORY, filename);

                if (write_wav_file(final_file_path, data->buffer, data->size, SAMPLE_RATE) == 0)
                    printf("Saved: %s\n", final_file_path);
                else
                    fprintf(stderr, "Write failed.\n");
            }
            free(data->buffer);
            data->buffer = NULL;
            data->size = 0; data->capacity = 0; data->recording = 0;
        }
    }
    return paContinue;
}

int findInputDeviceByName(const char *name)
{
    int numDevices = Pa_GetDeviceCount();
    for (int i = 0; i < numDevices; i++)
    {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        if (info && info->maxInputChannels > 0 && strstr(info->name, name) != NULL) return i;
    }
    return paNoDevice;
}

void recorder(const char *com_port)
{
    PaError err;
    PaStream *stream;
    AudioData data = {0};

    if (load_env(".env") != 0) printf("Env load failed\n");

    data.amplitude_threshold = AMPLITUDE_THRESHOLD;
    data.chunk_size = CHUNK_SIZE;
    
    // --- CONFIG ---
    int wants_live_listen = LIVE_LISTEN; // Value from config file
    const char *AUDIO_DEVICE_NAME = "hw:0,0"; 
    char *serial_name = "radio"; 
    snprintf(data.serial_name, sizeof(data.serial_name), "%s", serial_name ? serial_name : "unknown");

    err = Pa_Initialize();
    if (err != paNoError) { fprintf(stderr, "PA Init Error: %s\n", Pa_GetErrorText(err)); return; }

    // --- SETUP INPUT ---
    PaStreamParameters inputParams;
    inputParams.device = findInputDeviceByName(AUDIO_DEVICE_NAME);
    if (inputParams.device == paNoDevice) {
        // Fallback: Try default input if hw:0,0 fails
        printf("Warning: '%s' not found. Trying default input.\n", AUDIO_DEVICE_NAME);
        inputParams.device = Pa_GetDefaultInputDevice();
    }
    
    if (inputParams.device == paNoDevice) {
        fprintf(stderr, "Error: No input device found.\n");
        Pa_Terminate(); return;
    }
    
    printf("Input Device: %s\n", Pa_GetDeviceInfo(inputParams.device)->name);
    inputParams.channelCount = CHANNELS;
    inputParams.sampleFormat = paInt16;
    inputParams.suggestedLatency = Pa_GetDeviceInfo(inputParams.device)->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = NULL;

    // --- ATTEMPT 1: OPEN INPUT + OUTPUT (LIVE LISTEN) ---
    int stream_opened = 0;

    if (wants_live_listen)
    {
        PaStreamParameters outputParams;
        outputParams.device = Pa_GetDefaultOutputDevice();
        
        if (outputParams.device != paNoDevice) {
            outputParams.channelCount = CHANNELS;
            outputParams.sampleFormat = paInt16;
            outputParams.suggestedLatency = Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;
            outputParams.hostApiSpecificStreamInfo = NULL;
            
            printf("Attempting Live Listen with Output: %s\n", Pa_GetDeviceInfo(outputParams.device)->name);
            
            err = Pa_OpenStream(&stream, &inputParams, &outputParams, SAMPLE_RATE, data.chunk_size, paClipOff, audioCallback, &data);
            
            if (err == paNoError) {
                printf("SUCCESS: Audio routing to speakers ENABLED.\n");
                data.live_listen_active = 1;
                stream_opened = 1;
            } else {
                fprintf(stderr, "WARNING: Live Listen failed (Error: %s). Retrying recording ONLY.\n", Pa_GetErrorText(err));
            }
        } else {
            printf("WARNING: No speakers found. Skipping Live Listen.\n");
        }
    }

    // --- ATTEMPT 2: FALLBACK TO INPUT ONLY (RECORDING ONLY) ---
    if (!stream_opened)
    {
        data.live_listen_active = 0;
        // Pass NULL for output params
        err = Pa_OpenStream(&stream, &inputParams, NULL, SAMPLE_RATE, data.chunk_size, paClipOff, audioCallback, &data);
        
        if (err != paNoError) {
            fprintf(stderr, "CRITICAL ERROR: Could not open recording stream either! %s\n", Pa_GetErrorText(err));
            Pa_Terminate();
            return;
        }
        printf("SUCCESS: Recording stream opened (No Speakers).\n");
    }

    err = Pa_StartStream(stream);
    if (err != paNoError) { fprintf(stderr, "Start Error: %s\n", Pa_GetErrorText(err)); Pa_CloseStream(stream); Pa_Terminate(); return; }

    printf("Audio system active. Waiting for sound...\n");

    while (1) sleep(1);

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
}