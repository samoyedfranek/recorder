#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <portaudio.h>

// --- Configuration Constants ---
#define SAMPLE_RATE 48000
#define CHANNELS 1
#define CHUNK_SIZE 1024 // Frames per buffer
#define AMPLITUDE_THRESHOLD 300
#define SILENCE_THRESHOLD 5   // Seconds of silence before stopping
#define REMOVE_LAST_SECONDS 5 // Seconds to remove from the end of the recording

// --- Audio Data Structure ---
typedef struct
{
    int recording;
    time_t last_sound_time;
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

    return paContinue;
}

// --- Logger Function ---
void recorder()
{
    PaError err;
    PaStream *stream;
    AudioData data = {0};

    err = Pa_Initialize();
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio init error: %s\n", Pa_GetErrorText(err));
        return;
    }

    err = Pa_OpenDefaultStream(&stream, CHANNELS, 0, paInt16, SAMPLE_RATE,
                               CHUNK_SIZE, audioCallback, &data);
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio stream error: %s\n", Pa_GetErrorText(err));
        Pa_Terminate();
        return;
    }

    err = Pa_StartStream(stream);
    if (err != paNoError)
    {
        fprintf(stderr, "PortAudio start error: %s\n", Pa_GetErrorText(err));
        Pa_CloseStream(stream);
        Pa_Terminate();
        return;
    }

    printf("Logging audio data... Press Ctrl+C to stop.\n");
    while (1)
    {
        sleep(1);
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
}
