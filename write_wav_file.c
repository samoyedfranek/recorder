#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "h/open_serial_port.h"

typedef struct
{
    char chunkID[4];
    uint32_t chunkSize;
    char format[4];
    char subchunk1ID[4];
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char subchunk2ID[4];
    uint32_t subchunk2Size;
} WAVHeader;

int write_wav_file(const char *filename, short *data, size_t numSamples, int sampleRate)
{
    FILE *file = fopen(filename, "wb");
    if (!file)
    {
        fprintf(stderr, "Error: Could not open file for writing: %s\n", filename);
        return -1;
    }

    WAVHeader header;
    memcpy(header.chunkID, "RIFF", 4);
    memcpy(header.format, "WAVE", 4);
    memcpy(header.subchunk1ID, "fmt ", 4);
    memcpy(header.subchunk2ID, "data", 4);

    header.subchunk1Size = 16;
    header.audioFormat = 1;
    header.numChannels = 1;
    header.sampleRate = sampleRate;
    header.bitsPerSample = 16;
    header.blockAlign = (header.numChannels * header.bitsPerSample) / 8;
    header.byteRate = header.sampleRate * header.blockAlign;
    header.subchunk2Size = numSamples * sizeof(short);
    header.chunkSize = 36 + header.subchunk2Size;

    fwrite(&header, sizeof(WAVHeader), 1, file);
    fwrite(data, sizeof(short), numSamples, file);

    fclose(file);
    return 0;
}
