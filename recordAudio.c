#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <alsa/asoundlib.h>

// --- Configuration Constants ---
#define PCM_DEVICE "hw:1,0" // Use the appropriate device name
#define RATE 48000
#define CHANNELS 1
#define SAMPLE_SIZE 2   // 16-bit samples (2 bytes per sample)
#define CHUNK_SIZE 1024 // Number of frames to capture per read
#define AMPLITUDE_THRESHOLD 300
#define SILENCE_THRESHOLD 5 // Seconds of silence before stopping

// Directories for saving files
#define CACHE_DIR "./cache"
#define RECORDINGS_DIR "./recordings"

// --- Forward declaration ---
extern char *open_serial_port(const char *com_port);

// --- Recorder Function ---
void recorder(const char *com_port)
{
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    unsigned int rate = RATE;
    int dir;
    short *audio_buffer;
    int frames;
    int recording = 0;
    time_t last_sound_time = 0;
    size_t audio_buffer_size = 0;
    size_t audio_buffer_capacity = 0;

    // Open the PCM device for capture
    if (snd_pcm_open(&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_CAPTURE, 0) < 0)
    {
        fprintf(stderr, "Error opening PCM device %s\n", PCM_DEVICE);
        return;
    }

    // Allocate hardware parameters structure
    snd_pcm_hw_params_malloc(&params);

    // Set default hardware parameters
    snd_pcm_hw_params_any(pcm_handle, params);
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm_handle, params, CHANNELS);
    snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, &dir);

    // Apply the hardware parameters
    if (snd_pcm_hw_params(pcm_handle, params) < 0)
    {
        fprintf(stderr, "Error setting PCM parameters\n");
        return;
    }

    // Allocate a buffer to hold the audio data
    audio_buffer = (short *)malloc(CHUNK_SIZE * sizeof(short));
    if (!audio_buffer)
    {
        fprintf(stderr, "Error allocating buffer\n");
        return;
    }

    // Open serial port for filename
    char *serial_name = open_serial_port(com_port);
    if (!serial_name)
    {
        serial_name = "unknown";
    }

    printf("Recording loop started...\n");
    while (1)
    {
        // Capture audio data from PCM device
        frames = snd_pcm_readi(pcm_handle, audio_buffer, CHUNK_SIZE);
        if (frames < 0)
        {
            fprintf(stderr, "Error reading from PCM device: %s\n", snd_strerror(frames));
            break;
        }

        // Compute the maximum amplitude of the captured chunk
        int max_amplitude = 0;
        for (int i = 0; i < frames; i++)
        {
            int sample = abs(audio_buffer[i]);
            if (sample > max_amplitude)
                max_amplitude = sample;
        }

        // Check if the amplitude exceeds the threshold to start or continue recording
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
                audio_buffer = (short *)realloc(audio_buffer, audio_buffer_capacity * sizeof(short));
                if (!audio_buffer)
                {
                    fprintf(stderr, "Memory allocation failed\n");
                    break;
                }
            }

            // Add the captured chunk to the audio buffer
            if (audio_buffer_size + frames > audio_buffer_capacity)
            {
                audio_buffer_capacity *= 2;
                audio_buffer = realloc(audio_buffer, audio_buffer_capacity * sizeof(short));
                if (!audio_buffer)
                {
                    fprintf(stderr, "Memory reallocation failed\n");
                    break;
                }
            }
            memcpy(audio_buffer + audio_buffer_size, audio_buffer, frames * sizeof(short));
            audio_buffer_size += frames;
            last_sound_time = current_time;
        }
        else if (recording)
        {
            // Add silence to the buffer if recording and no sound detected
            short silence[CHUNK_SIZE] = {0};
            if (audio_buffer_size + frames > audio_buffer_capacity)
            {
                audio_buffer_capacity *= 2;
                audio_buffer = realloc(audio_buffer, audio_buffer_capacity * sizeof(short));
                if (!audio_buffer)
                {
                    fprintf(stderr, "Memory reallocation failed\n");
                    break;
                }
            }
            memcpy(audio_buffer + audio_buffer_size, silence, frames * sizeof(short));
            audio_buffer_size += frames;

            // Check for silence threshold to stop recording
            if (difftime(current_time, last_sound_time) > SILENCE_THRESHOLD)
            {
                printf("Silence detected. Saving recording...\n");

                size_t cut_samples = RATE * 4; // Cut the last 4 seconds of audio
                if (audio_buffer_size > cut_samples)
                {
                    audio_buffer_size -= cut_samples;
                }
                else
                {
                    audio_buffer_size = 0;
                }

                // Save the recording to a file
                if (audio_buffer_size > 0)
                {
                    char filename[256], final_file_path[256];
                    char time_str[64];
                    time_t now = time(NULL);
                    struct tm *t = localtime(&now);
                    strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", t);
                    snprintf(filename, sizeof(filename), "%s_%s.wav", serial_name, time_str);
                    snprintf(final_file_path, sizeof(final_file_path), RECORDINGS_DIR "/%s", filename);

                    // Assuming write_wav_file function is available to write the WAV file
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

        usleep(10000); // Small delay to prevent CPU overload
    }

    // Cleanup
    snd_pcm_close(pcm_handle);
    free(audio_buffer);
}
