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

    // --- LIVE LISTEN LOGIC ---
    // Only process output if the stream was opened with output enabled
    if (output)
    {
        if (data->live_listen && input)
        {
            // Pass-through: Copy microphone input directly to speakers
            memcpy(output, input, framesPerBuffer * sizeof(short));
        }
        else
        {
            // Mute output to prevent static/noise if live listen is off or input dropped
            memset(output, 0, framesPerBuffer * sizeof(short));
        }
    }
    // -------------------------

    if (!input)
    {
        // No input to record
        return paContinue;
    }

    // --- RECORDING LOGIC STARTS HERE ---
    
    // 1. Update Prebuffer
    for (unsigned int i = 0; i < framesPerBuffer; i++)
    {
        data->prebuffer[data->prebuffer_index] = input[i];
        data->prebuffer_index = (data->prebuffer_index + 1) % PREBUFFER_SIZE;
    }
    if (data->prebuffer_index == 0)
        data->prebuffer_full = 1;

    // 2. Check Amplitude
    int max_amplitude = 0;
    for (unsigned int i = 0; i < framesPerBuffer; i++)
    {
        int sample = abs(input[i]);
        if (sample > max_amplitude)
            max_amplitude = sample;
    }
    time_t current_time = time(NULL);

    // 3. Start Recording Trigger
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

        // Copy prebuffer
        size_t pre_count = data->prebuffer_full ? PREBUFFER_SIZE : data->prebuffer_index;
        size_t start_index = data->prebuffer_index; 

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

    // 4. Continue Recording
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
            // Optional: Print status
            // printf("Recording... Samples: %zu\n", data->size);
        }

        if (max_amplitude > data->amplitude_threshold)
        {
            data->last_sound_time = current_time;
        }

        // 5. Stop Recording Logic
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
    if (numDevices < 0) return paNoDevice;

    for (int i = 0; i < numDevices; i++)
    {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        if (!info) continue;
        if (info->maxInputChannels > 0 && strstr(info->name, name) != NULL)
            return i;
    }
    return paNoDevice;
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

    // Load config values
    data.amplitude_threshold = AMPLITUDE_THRESHOLD;
    data.chunk_size = CHUNK_SIZE;
    data.recording_total_chunks = 0;
    data.live_listen = LIVE_LISTEN; // Using the value from .env/config.h

    const char *AUDIO_DEVICE_NAME = "hw:0,0"; 
    
    char *serial_name = "radio"; 
    snprintf(data.serial_name, sizeof(data.serial_name), "%s", serial_name ? serial_name : "unknown");

    printf("Started recording on serial: %s\n", data.serial_name);
    printf("Looking for audio device: %s\n", AUDIO_DEVICE_NAME);

    if (data.live_listen) {
        printf("Live Listen ENABLED in config\n");
    } else {
        printf("Live Listen DISABLED in config\n");
    }

    err = Pa_Initialize();
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio init error: %s\n", Pa_GetErrorText(err));
        return;
    }

    // --- INPUT SETUP ---
    PaStreamParameters inputParams;
    int inputDeviceIndex = findInputDeviceByName(AUDIO_DEVICE_NAME);

    if (inputDeviceIndex == paNoDevice)
    {
        fprintf(stderr, "Error: Could not find input device matching '%s'.\n", AUDIO_DEVICE_NAME);
        Pa_Terminate();
        return;
    }
    
    printf("Found input device: %s\n", Pa_GetDeviceInfo(inputDeviceIndex)->name);
    inputParams.device = inputDeviceIndex;
    inputParams.channelCount = CHANNELS;
    inputParams.sampleFormat = paInt16;
    inputParams.suggestedLatency = Pa_GetDeviceInfo(inputParams.device)->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = NULL;

    // --- OUTPUT SETUP (Conditional) ---
    PaStreamParameters outputParams;
    PaStreamParameters *pOutputParams = NULL; // Default to NULL (No output)

    if (data.live_listen)
    {
        outputParams.device = Pa_GetDefaultOutputDevice();
        if (outputParams.device == paNoDevice)
        {
            fprintf(stderr, "Warning: Live listen is TRUE but no default output device found.\n");
        }
        else
        {
            outputParams.channelCount = CHANNELS;
            outputParams.sampleFormat = paInt16;
            outputParams.suggestedLatency = Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;
            outputParams.hostApiSpecificStreamInfo = NULL;
            
            // Assign the pointer so Pa_OpenStream uses it
            pOutputParams = &outputParams;
            printf("Output Device (Live Listen): %s\n", Pa_GetDeviceInfo(outputParams.device)->name);
        }
    }

    // --- OPEN STREAM ---
    // We pass pOutputParams. If it is NULL, PortAudio opens an input-only stream.
    // If it points to valid params, PortAudio opens a Full Duplex stream.
    err = Pa_OpenStream(&stream, 
                        &inputParams, 
                        pOutputParams, 
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

    // Keep program running
    while (1)
    {
        sleep(1);
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
}