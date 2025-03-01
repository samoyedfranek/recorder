import os
import wave
import json
from datetime import datetime
from serialReader import open_serial_port
import pyaudio
import numpy as np
from multiprocessing import Process, Queue


def trim_audio(frames, trim_seconds, rate):
    trim_samples = trim_seconds * rate * 2
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


def load_config():
    with open("config.json", "r") as f:
        return json.load(f)


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
        print(f"File saved: {file_path} (Trimmed 4s from end)")


def audio_recorder(queue, input_device_id, com_port, debug):
    CHUNK = 1024
    FORMAT = pyaudio.paInt16
    CHANNELS = 1
    RATE = 48000
    AMPLITUDE_THRESHOLD = 200
    SILENCE_DURATION = 5
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
                silent_chunks = 0
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
                    queue.put((frames, f"{filename}_{datetime.now().strftime('%Y%m%d_%H%M%S')}"))
                    frames.clear()
                    recording = False

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


def audio_processor(queue, rate, channels, format_, debug):
    while True:
        frames, filename = queue.get()  # Blocking call
        if frames is None:
            break  # Exit signal received
        save_audio_file(frames, filename, rate, channels, format_, debug)


def start():
    config = load_config()
    input_device_id = config["input_device"]
    com_port = config["com_port"]
    debug = config.get("debug", False)

    queue = Queue()  # Inter-process communication queue

    recorder_process = Process(target=audio_recorder, args=(queue, input_device_id, com_port, debug))
    processor_process = Process(target=audio_processor, args=(queue, 48000, 1, pyaudio.paInt16, debug))

    recorder_process.start()
    processor_process.start()

    try:
        recorder_process.join()
    except KeyboardInterrupt:
        queue.put((None, None))  # Signal processor to exit
        processor_process.join()


if __name__ == "__main__":
    start()
