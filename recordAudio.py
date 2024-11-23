import os
import time
import wave
import json
from queue import Queue
from threading import Thread
from datetime import datetime
import pyaudio
from googleDrive import authenticate_google_drive, upload_to_google_drive

def record(filename):
    # Load configuration from JSON
    def load_config():
        with open('config.json', 'r') as f:
            return json.load(f)

    config = load_config()
    input_device_id = config["input_device"]

    CHUNK = 1024
    FORMAT = pyaudio.paInt16
    CHANNELS = 1
    RATE = 44100
    SILENCE_THRESHOLD = 500
    SILENCE_DURATION = 5

    LOCAL_STORAGE_PATH = "./recordings"
    os.makedirs(LOCAL_STORAGE_PATH, exist_ok=True)

    upload_queue = Queue()
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
        return max(data) < SILENCE_THRESHOLD

    def record_audio():
        p = pyaudio.PyAudio()
        try:
            input_stream = p.open(format=FORMAT, channels=CHANNELS, rate=RATE, input=True, input_device_index=input_device_id, frames_per_buffer=CHUNK)
        except IOError as e:
            print(f"Error opening audio stream: {e}")
            return

        print("Listening for sound...")
        frames = []
        silent_chunks = 0
        recording = False

        while True:
            try:
                data = input_stream.read(CHUNK, exception_on_overflow=False)
            except IOError as e:
                print(f"Error reading audio data: {e}")
                continue

            audio_data = wave.struct.unpack("%dh" % (len(data) // 2), data)

            if not is_silent(audio_data):
                if not recording:
                    print("Sound detected, recording started...")
                recording = True
                silent_chunks = 0
                frames.append(data)
                audio_queue.put(data)
            elif recording:
                silent_chunks += 1
                if silent_chunks >= (SILENCE_DURATION * RATE / CHUNK):
                    print("Silence detected, recording stopped.")
                    recording = False
                    file_name = f"{filename}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.wav"
                    file_path = save_audio_file(frames, file_name)
                    upload_queue.put(file_path)
                    frames.clear()


    def process_uploads(service):
        while True:
            file_path = upload_queue.get()
            if file_path is None:
                break
            upload_to_google_drive(file_path, '1eEMhEHFETEi8uQec5OhrmWdF-RZfxUAy', service)
            time.sleep(1)

    drive_service = authenticate_google_drive()

    upload_thread = Thread(target=process_uploads, args=(drive_service,), daemon=True)
    upload_thread.start()

    try:
        record_audio()
    except KeyboardInterrupt:
        print("Program terminated.")
        upload_queue.put(None)
        upload_thread.join()
    except Exception as e:
        print(f"Error: {e}")
