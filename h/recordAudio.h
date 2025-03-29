#ifndef RECORDAUDIO_H
#define RECORDAUDIO_H

// recorder starts the audio capture loop using the specified input device and COM port.
// 'debug' is a flag for extra logging.
void recorder(const char *com_port);

#endif // RECORDAUDIO_H
