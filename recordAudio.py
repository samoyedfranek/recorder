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

    CHUNK = 1024
    FORMAT = pyaudio.paInt16
    CHANNELS = 1
    RATE = 48000
    SILENCE_THRESHOLD = 200
    SILENCE_DURATION = 5

    LOCAL_STORAGE_PATH = "./recordings"
    os.makedirs(LOCAL_STORAGE_PATH, exist_ok=True)

    audio_queue = Queue()

    def save_audio_file(frames, file_name):
        file_path = os.path.join(LOCAL_STORAGE_PATH, file_name)
        with wave.open(file_path, 'wb') as wf:
            wf.setnchannels(CHANNELS)
            wf.setsampwidth(pyaudio.PyAudio().get_sample_size(FORMAT))
            wf.setframerate(RATE)
            wf.writeframes(b''.join(frames))
        return file_path

    def is_silent(data):
        # Unpack the audio data into integers
        audio_data = wave.struct.unpack("%dh" % (len(data) // 2), data)
        max_amplitude = max(abs(i) for i in audio_data)
        print(f"Max amplitude: {max_amplitude}")  # Debug: print the max amplitude
        return max_amplitude < SILENCE_THRESHOLD

    def record_audio():
        p = pyaudio.PyAudio()

        # Open the input stream for recording
        input_stream = p.open(format=FORMAT, channels=CHANNELS, rate=RATE, input=True, input_device_index=input_device_id, frames_per_buffer=CHUNK)

        print("Listening for sound...")
        frames = []
        silent_chunks = 0
        recording = False

        while True:
            try:
                # Read audio data from the input stream
                data = input_stream.read(CHUNK, exception_on_overflow=False)
            except IOError as e:
                print(f"Error reading audio data: {e}")
                continue

            # Check if the audio is silent
            if not is_silent(data):
                if not recording:
                    print("Sound detected, recording started...")
                    filename = open_serial_port(com_port)
                recording = True
                silent_chunks = 0
                frames.append(data)
                audio_queue.put(data)
            elif recording:
                silent_chunks += 1
                # If silence is detected for the set duration, stop recording
                if silent_chunks >= (SILENCE_DURATION * RATE / CHUNK):
                    print("Silence detected, recording stopped.")
                    recording = False
                    file_name = f"{filename}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.wav"
                    save_audio_file(frames, file_name)
                    frames.clear()

    try:
        record_audio()
    except KeyboardInterrupt:
        print("Program terminated.")
    except Exception as e:
        print(f"Error: {e}")
