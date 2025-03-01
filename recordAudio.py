import os
import wave
import json
from datetime import datetime
from serialReader import open_serial_port
import pyaudio
import numpy as np
from multiprocessing import Process


def load_config():
    with open("config.json", "r") as f:
        return json.load(f)


def save_audio_file(frames, file_name, rate, channels, format_, debug):
    if not frames:
        if debug:
            print("No audio data to save. Skipping file.")
        return

    file_path = f"./recordings/{file_name}.wav"
    os.makedirs(os.path.dirname(file_path), exist_ok=True)

    with wave.open(file_path, "wb") as wf:
        wf.setnchannels(channels)
        wf.setsampwidth(pyaudio.PyAudio().get_sample_size(format_))
        wf.setframerate(rate)
        wf.writeframes(b"".join(frames))

    if debug:
        print(f"File saved: {file_path}")


def audio_recorder(input_device_id, com_port, debug):
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
                    save_audio_file(frames, f"{filename}_{datetime.now().strftime('%Y%m%d_%H%M%S')}", RATE, CHANNELS, FORMAT, debug)
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


def start():
    config = load_config()
    input_device_id = config["input_device"]
    com_port = config["com_port"]
    debug = config.get("debug", False)

    if debug:
        print("Starting recording process...")

    # Start recorder as a separate process
    recorder_process = Process(target=audio_recorder, args=(input_device_id, com_port, debug))
    recorder_process.start()
    recorder_process.join()  # Ensure the process completes before exiting

    if debug:
        print("Recording process ended.")


if __name__ == "__main__":
    start()
