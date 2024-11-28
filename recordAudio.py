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
    SILENCE_THRESHOLD = 1000  # Threshold for silence detection
    SILENCE_DURATION = 5  # Duration for detecting sustained silence
    MINIMUM_VALID_AMPLITUDE = 1000  # Minimum valid amplitude to start recording
    AMPLITUDE_DROP_THRESHOLD = 300  # Minimum amplitude drop to consider aborting
    AMPLITUDE_DROP_CHECK_DURATION = 1  # Duration in seconds to check amplitude drop

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
        print(f"Audio saved as {file_name}")
        return file_path

    def is_silent(data):
        # Unpack the audio data into integers
        audio_data = wave.struct.unpack("%dh" % (len(data) // 2), data)
        max_amplitude = max(abs(i) for i in audio_data)
        print(f"Max amplitude: {max_amplitude}")  # Debug: Print the max amplitude
        return max_amplitude < SILENCE_THRESHOLD

    def record_audio():
        p = pyaudio.PyAudio()

        # Open the input stream for recording
        input_stream = p.open(format=FORMAT, channels=CHANNELS, rate=RATE, input=True, input_device_index=input_device_id, frames_per_buffer=CHUNK)

        print("Listening for sound...")
        frames = []
        silent_chunks = 0
        recording = False
        initial_amplitude = 0
        amplitude_history = []  # Track amplitude history for 1 second
        amplitude_drop_start_time = time.time()  # Time when the drop check started

        while True:
            try:
                # Read audio data from the input stream
                data = input_stream.read(CHUNK, exception_on_overflow=False)
            except IOError as e:
                print(f"Error reading audio data: {e}")
                continue

            # Check if the audio is silent
            max_amplitude = max(abs(i) for i in wave.struct.unpack("%dh" % (len(data) // 2), data))

            if max_amplitude >= MINIMUM_VALID_AMPLITUDE:
                if not recording:
                    print("Sound detected, recording started...")
                    filename = open_serial_port(com_port)
                recording = True
                silent_chunks = 0
                frames.append(data)
                audio_queue.put(data)
                initial_amplitude = max_amplitude  # Record the initial high amplitude
                amplitude_history = [initial_amplitude]  # Start tracking amplitude
                amplitude_drop_start_time = time.time()  # Reset the start time for drop check
            elif recording:
                silent_chunks += 1
                frames.append(data)
                audio_queue.put(data)

                # Check if amplitude has dropped significantly over the last second
                amplitude_history.append(max_amplitude)
                if time.time() - amplitude_drop_start_time > AMPLITUDE_DROP_CHECK_DURATION:
                    amplitude_history = amplitude_history[-int(RATE / CHUNK)]  # Keep only the last 1 second of data
                    max_amplitude_in_history = max(amplitude_history)
                    if initial_amplitude - max_amplitude_in_history > AMPLITUDE_DROP_THRESHOLD:
                        print(f"Amplitude dropped significantly from {initial_amplitude} to {max_amplitude_in_history}, discarding recording.")
                        recording = False
                        frames.clear()
                        amplitude_history.clear()
                        continue

                # If silence is detected for the set duration, stop recording
                if silent_chunks >= (SILENCE_DURATION * RATE / CHUNK):
                    print("Silence detected, recording stopped.")
                    recording = False
                    file_name = f"{filename}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.wav"
                    save_audio_file(frames, file_name)
                    frames.clear()
                    amplitude_history.clear()

    try:
        record_audio()
    except KeyboardInterrupt:
        print("Program terminated.")
    except Exception as e:
        print(f"Error: {e}")
