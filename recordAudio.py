import os
import time
import numpy as np
import wave
import json
import threading
import pyaudio
from datetime import datetime
from serialReader import open_serial_port
import shutil


def move_file_to_recordings(temp_file_path, final_file_path):
    shutil.move(temp_file_path, final_file_path)
    print(f"File moved to: {final_file_path}")


def save_audio_chunk(wave_file, audio_frames, debug, RATE):
    if audio_frames.size == 0:
        return

    cut_samples = RATE * 5
    if len(audio_frames) > cut_samples:
        audio_frames = audio_frames[:-cut_samples]

    wave_file.writeframes(audio_frames.astype(np.int16).tobytes())

    if debug:
        print(f"Writing {len(audio_frames)} samples to file")


def recorder(input_device_id, com_port, debug):
    RATE = 48000
    AMPLITUDE_THRESHOLD = 200
    SILENCE_THRESHOLD = 5
    CHUNK_SIZE = 1024

    audio_frames = np.array([])  
    last_sound_time = None
    recording = [False]  
    temp_file_path = None
    wf = None

    p = pyaudio.PyAudio()
    stream = p.open(format=pyaudio.paInt16, channels=1, rate=RATE, input=True, input_device_index=input_device_id, frames_per_buffer=CHUNK_SIZE)

    serial_name = open_serial_port(com_port)

    stream_active = True  # Track stream status

    try:
        while True:
            indata = np.frombuffer(stream.read(CHUNK_SIZE), dtype=np.int16)

            max_amplitude = np.max(np.abs(indata))

            if debug:
                print(f"Current max amplitude: {max_amplitude}")

            if max_amplitude > AMPLITUDE_THRESHOLD:
                if not recording[0]:
                    print("Recording started.")
                    filename = f"{serial_name}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.wav"
                    temp_file_path = f"./cache/{filename}"
                    os.makedirs(os.path.dirname(temp_file_path), exist_ok=True)

                    wf = wave.open(temp_file_path, "wb")
                    wf.setnchannels(1)
                    wf.setsampwidth(2)
                    wf.setframerate(RATE)

                    save_audio_chunk(wf, indata, debug, RATE)

                    recording[0] = True
                    last_sound_time = time.time()
                    audio_frames = indata

                else:
                    audio_frames = np.concatenate((audio_frames, indata))
                    last_sound_time = time.time()

            elif recording[0]:
                audio_frames = np.concatenate((audio_frames, indata))

                if time.time() - last_sound_time > SILENCE_THRESHOLD:
                    print(f"Silence detected for {SILENCE_THRESHOLD} seconds. Saving audio file: {temp_file_path}")
                    save_audio_chunk(wf, audio_frames, debug, RATE)

                    wf.close()

                    final_file_path = f"./recordings/{filename}"
                    move_file_to_recordings(temp_file_path, final_file_path)

                    audio_frames = np.array([])  
                    recording[0] = False
                    last_sound_time = None

            elif max_amplitude <= AMPLITUDE_THRESHOLD:
                last_sound_time = time.time()

    except KeyboardInterrupt:
        print("Recording stopped.")

    finally:
        # Safely handle stream closing
        if stream_active:
            try:
                stream.stop_stream()
                stream.close()
                print("Stream stopped successfully.")
            except Exception as e:
                print(f"Error stopping the stream: {e}")

        if wf is not None:
            wf.close()


def start_recording(input_device_id, com_port, debug):
    recorder_thread = threading.Thread(target=recorder, args=(input_device_id, com_port, debug), daemon=True)
    recorder_thread.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("Recording stopped.")
        recorder_thread.join()


def main():
    config = json.load(open("config.json"))
    input_device_id = config["input_device"]
    com_port = open_serial_port(config["com_port"])
    debug = config.get("debug", False)

    print(f"Starting recording on device {input_device_id} with COM port {com_port}")
    start_recording(input_device_id, com_port, debug)


if __name__ == "__main__":
    main()
