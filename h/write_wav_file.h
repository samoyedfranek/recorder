#ifndef WRITE_WAV_FILE_H
#define WRITE_WAV_FILE_H

#include <stddef.h>

int write_wav_file(const char *filename, short *data, size_t numSamples, int sampleRate);

#endif
