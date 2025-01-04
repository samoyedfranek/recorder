import os
import wave
import json
from datetime import datetime
from serialReader import open_serial_port
import pyaudio
import numpy as np

def record():
    # Load configuration from JSON
    def load_config():
        with open('config.json', 'r') as f:
            return json.load(f)

    config = load_config()
    input_device_id = config["input_device"]
    com_port = config["com_port"]

    CHUNK = 16384
    FORMAT = pyaudio.paInt24
    CHANNELS = 1
    RATE = 48000
    SILENCE_THRESHOLD = 275  # Increased threshold for silence detection
    SILENCE_DURATION = 3  # Adjusted to a smaller duration to allow more audio before stopping

    LOCAL_STORAGE_PATH = "./recordings"
    CACHE_DIR = './cache'  # Path for short recordings
    os.makedirs(LOCAL_STORAGE_PATH, exist_ok=True)
    os.makedirs(CACHE_DIR, exist_ok=True)  # Create cache directory if not exists

    def is_silent(data):
        audio_data = np.frombuffer(data, dtype=np.int16)
        max_amplitude = np.max(np.abs(audio_data))
        return max_amplitude < SILENCE_THRESHOLD
        
    def save_audio_file(frames, file_name, is_short=False):
        if not frames:
            print("No audio data to save. Skipping file.")
            return
        # Decide where to save the file based on whether it's short or normal
        if is_short:
            file_path = os.path.join(CACHE_DIR, file_name)  # Save to cache if short
        else:
            file_path = os.path.join(LOCAL_STORAGE_PATH, file_name)  # Save to normal storage if not short

        with wave.open(file_path, 'wb') as wf:
            wf.setnchannels(CHANNELS)
            wf.setsampwidth(pyaudio.PyAudio().get_sample_size(FORMAT))
            wf.setframerate(RATE)
            wf.writeframes(b''.join(frames))
        print(f"File saved: {file_path}")

    def record_audio():
        p = pyaudio.PyAudio()
        input_stream = p.open(format=FORMAT, channels=CHANNELS, rate=RATE, input=True,
                            input_device_index=input_device_id, frames_per_buffer=CHUNK)
        print("Listening for sound...")
        frames = []
        silent_chunks = 0
        non_silent_chunks = 0  # Tracks the number of non-silent chunks
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
                    non_silent_chunks += 1  # Increment for non-silent chunks
                    frames.append(data)
                    total_non_silent_duration = non_silent_chunks * CHUNK / RATE
                elif recording:
                    silent_chunks += 1
                    frames.append(data)
                    if silent_chunks >= (SILENCE_DURATION * RATE / CHUNK):
                        print("Silence detected, recording stopped.")
                        recording = False

                        total_non_silent_duration = non_silent_chunks * CHUNK / RATE
                        if total_non_silent_duration >= 1:
                            # Normal recording
                            file_name = f"{filename}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.wav"
                            save_audio_file(frames, file_name, is_short=False)
                        else:
                            # Short recording saved to /cache
                            print(f"Recording too short ({total_non_silent_duration:.2f} sec). Saving to /cache.")
                            short_file_name = f"{filename}_{datetime.now().strftime('%Y%m%d_%H%M%S')}_short.wav"
                            save_audio_file(frames, short_file_name, is_short=True)

                        frames.clear()
                        non_silent_chunks = 0  # Reset non-silent chunk counter
                else:
                    frames.clear()  # Discard silent data when not recording
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
