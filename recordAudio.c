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
    double b0, b1, b2;
    double a1, a2;
    double z1, z2;
} Biquad;

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
    Biquad high_pass;
    Biquad low_pass;
    double signal_envelope;
    double gain;
    double envelope_attack;
    double envelope_release;
    double gain_release;
    size_t live_gate_hold_samples;
} AudioData;

static void initialize_biquad(Biquad *filter, int high_pass, double cutoff)
{
    const double pi = 3.14159265358979323846;
    const double omega = 2.0 * pi * cutoff / SAMPLE_RATE;
    const double cosine = cos(omega);
    const double alpha = sin(omega) / (2.0 * 0.7071067811865476);
    const double a0 = 1.0 + alpha;

    if (high_pass)
    {
        filter->b0 = ((1.0 + cosine) / 2.0) / a0;
        filter->b1 = (-(1.0 + cosine)) / a0;
        filter->b2 = ((1.0 + cosine) / 2.0) / a0;
    }
    else
    {
        filter->b0 = ((1.0 - cosine) / 2.0) / a0;
        filter->b1 = (1.0 - cosine) / a0;
        filter->b2 = ((1.0 - cosine) / 2.0) / a0;
    }

    filter->a1 = (-2.0 * cosine) / a0;
    filter->a2 = (1.0 - alpha) / a0;
    filter->z1 = 0.0;
    filter->z2 = 0.0;
}

static double process_biquad(Biquad *filter, double input)
{
    const double output = filter->b0 * input + filter->z1;
    filter->z1 = filter->b1 * input - filter->a1 * output + filter->z2;
    filter->z2 = filter->b2 * input - filter->a2 * output;
    return output;
}

static short process_audio_sample(AudioData *data, short input)
{
    double sample = process_biquad(&data->high_pass, input);
    sample = process_biquad(&data->low_pass, sample);

    const double magnitude = fabs(sample);
    const double envelope_coefficient = magnitude > data->signal_envelope
                                            ? data->envelope_attack
                                            : data->envelope_release;
    data->signal_envelope += envelope_coefficient * (magnitude - data->signal_envelope);

    double target_gain = 10000.0 / (data->signal_envelope + 1.0);
    if (target_gain < 0.5)
        target_gain = 0.5;
    else if (target_gain > 32.0)
        target_gain = 32.0;

    const double gain_coefficient = target_gain < data->gain
                                        ? data->envelope_attack
                                        : data->gain_release;
    data->gain += gain_coefficient * (target_gain - data->gain);

    sample *= data->gain;
    if (sample > 30000.0)
        sample = 30000.0;
    else if (sample < -30000.0)
        sample = -30000.0;

    return (short)sample;
}

static short live_output_sample(AudioData *data, short sample, int signal_detected)
{
    if (!data->live_listen)
        return 0;

    if (signal_detected)
    {
        data->live_gate_hold_samples = SAMPLE_RATE / 4;
    }
    else if (data->live_gate_hold_samples > 0)
    {
        data->live_gate_hold_samples--;
    }

    return signal_detected || data->live_gate_hold_samples > 0 ? sample : 0;
}

static int audioCallback(const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo *timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData)
{
    AudioData *data = (AudioData *)userData;
    const short *input = (const short *)inputBuffer;
    short *output = (short *)outputBuffer;

    if (!input)
    {
        if (output)
            memset(output, 0, framesPerBuffer * sizeof(short));
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
    time_t current_time = time(NULL);
    const int live_signal_detected = max_amplitude > data->amplitude_threshold;

    if (live_signal_detected && !data->recording)
    {
        data->recording = 1;
        data->recording_check_counter = 0;
        data->size = 0;

        char *actual_name = get_radio_name();
        if (actual_name)
        {
            strncpy(data->serial_name, actual_name, sizeof(data->serial_name) - 1);
            data->serial_name[sizeof(data->serial_name) - 1] = '\0';
            free(actual_name);
        }

        data->capacity = SAMPLE_RATE * 10;
        data->buffer = (short *)malloc(data->capacity * sizeof(short));
        if (!data->buffer)
        {
            fprintf(stderr, "Memory allocation failed!\n");
            return paAbort;
        }

        size_t pre_count = data->prebuffer_full ? PREBUFFER_SIZE : data->prebuffer_index;
        size_t start_index = data->prebuffer_full ? data->prebuffer_index : 0;

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
            size_t new_capacity = data->capacity;
            while (new_capacity < data->size + framesPerBuffer)
                new_capacity *= 2;
            short *new_buffer = realloc(data->buffer, new_capacity * sizeof(short));
            if (!new_buffer)
            {
                fprintf(stderr, "Memory reallocation failed!\n");
                return paAbort;
            }
            data->buffer = new_buffer;
            data->capacity = new_capacity;
        }

        for (unsigned long i = 0; i < framesPerBuffer; i++)
        {
            const short processed_sample = process_audio_sample(data, input[i]);
            data->prebuffer[data->prebuffer_index] = processed_sample;
            data->prebuffer_index = (data->prebuffer_index + 1) % PREBUFFER_SIZE;
            if (data->prebuffer_index == 0)
                data->prebuffer_full = 1;

            if (output)
                output[i] = live_output_sample(data, processed_sample, live_signal_detected);
            data->buffer[data->size++] = processed_sample;
        }
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
            printf("[RECORDING] Name: %s | DateTime: %s | Last sound: %s | Silence: %.2fs | Max Amplitude: %d | Chunks: %d | Samples: %zu | Recording time: %.2fs\n",
                   data->serial_name,
                   datetime_str,
                   last_sound_str,
                   silence_duration,
                   max_amplitude,
                   data->recording_total_chunks,
                   data->size,
                   recording_time_sec);
        }

        if (live_signal_detected)
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
                char filename[512], final_file_path[1024], time_str[64];
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
    else
    {
        for (unsigned long i = 0; i < framesPerBuffer; i++)
        {
            const short processed_sample = process_audio_sample(data, input[i]);
            data->prebuffer[data->prebuffer_index] = processed_sample;
            data->prebuffer_index = (data->prebuffer_index + 1) % PREBUFFER_SIZE;
            if (data->prebuffer_index == 0)
                data->prebuffer_full = 1;

            if (output)
                output[i] = live_output_sample(data, processed_sample, live_signal_detected);
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
    data.chunk_size = CHUNK_SIZE;
    data.recording_total_chunks = 0;
    data.live_listen = LIVE_LISTEN;
    data.gain = 1.0;
    data.envelope_attack = 1.0 - exp(-1.0 / (0.010 * SAMPLE_RATE));
    data.envelope_release = 1.0 - exp(-1.0 / (0.200 * SAMPLE_RATE));
    data.gain_release = 1.0 - exp(-1.0 / (0.350 * SAMPLE_RATE));
    initialize_biquad(&data.high_pass, 1, 180.0);
    initialize_biquad(&data.low_pass, 0, 4200.0);

    const char *AUDIO_DEVICE_NAME = "All-In-One-Cable";

    snprintf(data.serial_name, sizeof(data.serial_name), "radio");

    if (data.live_listen)
    {
        printf("Live Listen ENABLED (Outputting to default speakers)\n");
    }

    err = Pa_Initialize();
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio init error: %s\n", Pa_GetErrorText(err));
        return;
    }

    PaStreamParameters inputParams;
    int inputDeviceIndex = findInputDeviceByName(AUDIO_DEVICE_NAME);

    if (inputDeviceIndex == paNoDevice)
    {
        fprintf(stderr, "No default input device.\n");
        Pa_Terminate();
        return;
    }

    inputParams.device = inputDeviceIndex;
    inputParams.channelCount = CHANNELS;
    inputParams.sampleFormat = paInt16;
    inputParams.suggestedLatency = Pa_GetDeviceInfo(inputParams.device)->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = NULL;

    PaStreamParameters outputParams;
    PaStreamParameters *pOutputParams = NULL;

    if (data.live_listen)
    {
        outputParams.device = Pa_GetDefaultOutputDevice();
        if (outputParams.device == paNoDevice)
        {
            fprintf(stderr, "Warning: Live listen enabled but no output device found. Disabling live listen.\n");
            data.live_listen = 0;
        }
        else
        {
            outputParams.channelCount = CHANNELS;
            outputParams.sampleFormat = paInt16;
            outputParams.suggestedLatency = Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;
            outputParams.hostApiSpecificStreamInfo = NULL;
            pOutputParams = &outputParams;
        }
    }

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

    while (1)
    {
        sleep(1);
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
}