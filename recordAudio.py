import os
import time
import wave
import json
from queue import Queue
from threading import Thread
from datetime import datetime
from serialReader import open_serial_port
import pyaudio

def record():
    # Load configuration from JSON
    def load_config():
        with open('config.json', 'r') as f:
            return json.load(f)

    config = load_config()
    input_device_id = config["input_device"]
    com_port = config["com_port"]

    CHUNK = 2048
    FORMAT = pyaudio.paInt16
    CHANNELS = 1
    RATE = 48000
    SILENCE_THRESHOLD = 200  # Increased threshold for silence detection
    SILENCE_DURATION = 5  # Adjusted to a smaller duration to allow more audio before stopping

    LOCAL_STORAGE_PATH = "./recordings"
    os.makedirs(LOCAL_STORAGE_PATH, exist_ok=True)

    def is_silent(data):
        audio_data = wave.struct.unpack("%dh" % (len(data) // 2), data)
        max_amplitude = max(abs(i) for i in audio_data)
        print(f"Max amplitude: {max_amplitude}")
        return max_amplitude < SILENCE_THRESHOLD

    def save_audio_file(frames, file_name):
        if not frames:
            print("No audio data to save. Skipping file.")
            return
        file_path = os.path.join(LOCAL_STORAGE_PATH, file_name)
        with wave.open(file_path, 'wb') as wf:
            wf.setnchannels(CHANNELS)
            wf.setsampwidth(pyaudio.PyAudio().get_sample_size(FORMAT))
            wf.setframerate(RATE)
            wf.writeframes(b''.join(frames))
        total_frames = len(frames)
        expected_duration = total_frames * CHUNK / RATE
        print(f"Audio saved as {file_name}, Duration: {expected_duration:.2f} seconds, Total Frames: {total_frames}")

    def record_audio():
        p = pyaudio.PyAudio()
        input_stream = p.open(format=FORMAT, channels=CHANNELS, rate=RATE, input=True,
                            input_device_index=input_device_id, frames_per_buffer=CHUNK)
        print("Listening for sound...")
        frames = []
        silent_chunks = 0
        recording = False

        while True:
            try:
                data = input_stream.read(CHUNK, exception_on_overflow=False)
                if not is_silent(data):
                    if not recording:
                        print("Sound detected, recording started...")
                        filename = open_serial_port(com_port)
                    recording = True
                    silent_chunks = 0
                    frames.append(data)
                    print(f"Accumulated {len(frames)} frames")
                elif recording:
                    silent_chunks += 1
                    if silent_chunks >= (SILENCE_DURATION * RATE / CHUNK):
                        print("Silence detected, recording stopped.")
                        recording = False
                        file_name = f"{filename}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.wav"
                        save_audio_file(frames, file_name)
                        frames.clear()
            except IOError as e:
                print(f"Error reading audio data: {e}")
                continue
            except KeyboardInterrupt:
                print("Program terminated.")
                break
            except Exception as e:
                print(f"Error: {e}")


    try:
        record_audio()
    except KeyboardInterrupt:
        print("Program terminated.")
    except Exception as e:
        print(f"Error: {e}")
