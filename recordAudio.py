import os
import wave
import json
from datetime import datetime
from serialReader import open_serial_port
import pyaudio
import numpy as np


def trim_audio(frames, trim_seconds, rate, chunk_size):
    trim_frames = trim_seconds * rate // chunk_size
    return frames[:-trim_frames] if len(frames) > trim_frames else []


def record():
    def load_config():
        with open("config.json", "r") as f:
            return json.load(f)

    config = load_config()
    input_device_id = config["input_device"]
    com_port = config["com_port"]

    CHUNK = 8192
    FORMAT = pyaudio.paInt16
    CHANNELS = 1
    RATE = 48000
    SILENCE_THRESHOLD = 110  # Nagrywanie zaczyna się, gdy RMS < 110
    SILENCE_DURATION = 5
    TRIM_SECONDS = 5

    LOCAL_STORAGE_PATH = "./recordings"
    os.makedirs(LOCAL_STORAGE_PATH, exist_ok=True)

    def save_audio_file(frames, file_name):
        if not frames:
            print("No audio data to save. Skipping file.")
            return

        frames = trim_audio(frames, TRIM_SECONDS, RATE, CHUNK)
        file_path = os.path.join(LOCAL_STORAGE_PATH, file_name)

        with wave.open(file_path, "wb") as wf:
            wf.setnchannels(CHANNELS)
            wf.setsampwidth(pyaudio.PyAudio().get_sample_size(FORMAT))
            wf.setframerate(RATE)
            wf.writeframes(b"".join(frames))

        print(f"File saved: {file_path}")

    def record_audio():
        p = pyaudio.PyAudio()
        input_stream = p.open(
            format=FORMAT,
            channels=CHANNELS,
            rate=RATE,
            input=True,
            input_device_index=input_device_id,
            frames_per_buffer=CHUNK,
        )
        print("Listening for sound...")
        frames = []
        silent_chunks = 0
        recording = False

        try:
            while True:
                try:
                    data = input_stream.read(CHUNK, exception_on_overflow=False)
                    if not data:
                        print("Warning: Received empty audio data.")
                        continue
                except IOError as e:
                    print(f"Buffer overflow or device issue: {e}")
                    continue

                # Konwersja danych audio do numpy array
                audio_data = np.frombuffer(data, dtype=np.int16)
                if audio_data.size == 0:
                    print("Warning: Empty audio buffer received.")
                    continue

                # Obliczanie RMS i maksymalnej amplitudy
                rms_amplitude = np.sqrt(np.mean(audio_data**2)) if np.any(audio_data) else 0
                if np.isnan(rms_amplitude):
                    rms_amplitude = 0  # Zamiana NaN na 0

                max_amplitude = np.max(np.abs(audio_data)) if np.any(audio_data) else 0

                # Debugowanie wartości RMS
                # print(
                #     f"RMS Amplitude: {rms_amplitude}, Max Amplitude: {max_amplitude}, "
                #     f"Threshold: {SILENCE_THRESHOLD}, Silent chunks: {silent_chunks}, Recording: {recording}"
                # )

                if rms_amplitude < SILENCE_THRESHOLD:  # Nagrywanie startuje, gdy RMS < 110
                    if not recording:
                        print("Silence detected, recording started...")
                        filename = open_serial_port(com_port)
                    recording = True
                    silent_chunks = 0
                    frames.append(data)
                elif recording:
                    silent_chunks += 1
                    frames.append(data)
                    if silent_chunks >= (SILENCE_DURATION * RATE / CHUNK):
                        print("Sound detected, recording stopped.")
                        recording = False
                        file_name = f"{filename}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.wav"
                        save_audio_file(frames, file_name)
                        frames.clear()
                else:
                    frames.clear()

        except KeyboardInterrupt:
            print("Program terminated.")
        except Exception as e:
            print(f"Unexpected error: {e}")
        finally:
            input_stream.stop_stream()
            input_stream.close()
            p.terminate()
            print("Audio stream closed.")

    try:
        record_audio()
    except KeyboardInterrupt:
        print("Program terminated.")
    except Exception as e:
        print(f"Error: {e}")
