import os
import wave
import json
import time
from datetime import datetime
from serialReader import open_serial_port
import pyaudio
import numpy as np


def trim_audio(frames, trim_seconds, rate):
    trim_samples = trim_seconds * rate * 2  # 2 bytes per sample (16-bit audio)
    total_bytes = sum(len(f) for f in frames)

    if total_bytes <= trim_samples:
        return []

    keep_bytes = total_bytes - trim_samples
    new_frames, kept = [], 0

    for frame in frames:
        if kept + len(frame) <= keep_bytes:
            new_frames.append(frame)
            kept += len(frame)
        else:
            new_frames.append(frame[: keep_bytes - kept])
            break

    return new_frames


def save_audio_file(frames, file_name, rate, channels, format_, debug):
    if not frames:
        if debug:
            print("No audio data to save. Skipping file.")
        return

    frames = trim_audio(frames, trim_seconds=5, rate=rate)
    audio_data = np.frombuffer(b"".join(frames), dtype=np.int16)

    file_path = f"./recordings/{file_name}.wav"
    os.makedirs(os.path.dirname(file_path), exist_ok=True)

    with wave.open(file_path, "wb") as wf:
        wf.setnchannels(channels)
        wf.setsampwidth(pyaudio.PyAudio().get_sample_size(format_))
        wf.setframerate(rate)
        wf.writeframes(audio_data.tobytes())

    if debug:
        print(f"File saved: {file_path} (Trimmed 5s from end)")


def audio_recorder(input_device_id, com_port, debug):
    CHUNK = 2048  # Buffer size, adjust if necessary
    FORMAT = pyaudio.paInt16
    CHANNELS = 1
    RATE = 48000 # Adjusted to a more common rate
    AMPLITUDE_THRESHOLD = 200  # Lower threshold for detecting quieter sounds
    SILENCE_DURATION = 5  # Adjust to how long silence should be before stopping
    SILENCE_CHUNKS = int(SILENCE_DURATION * RATE / CHUNK)

    p = pyaudio.PyAudio()
    input_stream = p.open(
        format=FORMAT,
        channels=CHANNELS,
        rate=RATE,
        input=True,
        input_device_index=input_device_id,
        frames_per_buffer=CHUNK,
    )

    if debug:
        print("Listening for sound...")
    
    frames, silent_chunks, recording = [], 0, False
    filename = None

    try:
        while True:
            if not input_stream.is_active():
                time.sleep(0.01)  # Reduce CPU usage while waiting for data
                continue

            try:
                data = input_stream.read(CHUNK, exception_on_overflow=False)
                audio_data = np.frombuffer(data, dtype=np.int16)
                max_amplitude = np.max(np.abs(audio_data)) if audio_data.size > 0 else 0
            except IOError as e:
                if debug:
                    print(f"Buffer overflow or device issue: {e}")
                continue

            if debug:
                print(
                    f"Max Amplitude: {max_amplitude}, Threshold: {AMPLITUDE_THRESHOLD}, "
                    f"Silent Chunks: {silent_chunks}/{SILENCE_CHUNKS}, Recording: {recording}"
                )

            if max_amplitude > AMPLITUDE_THRESHOLD:
                silent_chunks = 0  # Reset silent chunks when sound is detected
                if not recording:
                    if debug:
                        print("Sound detected, recording started...")
                    filename = open_serial_port(com_port)
                    frames.clear()
                recording = True
                frames.append(data)
            elif recording:
                silent_chunks += 1
                frames.append(data)
                if silent_chunks >= SILENCE_CHUNKS:
                    if debug:
                        print("Silence detected, recording stopped.")
                    save_audio_file(frames, f"{filename}_{datetime.now().strftime('%Y%m%d_%H%M%S')}", RATE, CHANNELS, FORMAT, debug)
                    frames.clear()
                    recording = False
                    silent_chunks = 0  # Reset silent chunks after stopping recording

            time.sleep(0.05)  # Reduced delay to prevent 100% CPU usage

    except KeyboardInterrupt:
        if debug:
            print("Program terminated.")
    except Exception as e:
        if debug:
            print(f"Unexpected error: {e}")
    finally:
        input_stream.stop_stream()
        input_stream.close()
        p.terminate()
        if debug:
            print("Audio stream closed.")
