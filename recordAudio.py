import os
import time
import wave
import json
import threading
import pyaudio
import shutil
import numpy as np
from datetime import datetime
from serialReader import open_serial_port


def move_file_to_recordings(temp_file_path, final_file_path):
    """Moves the recorded file to the final directory."""
    shutil.move(temp_file_path, final_file_path)
    print(f"File moved to: {final_file_path}")


def recorder(input_device_id, com_port, debug):
    RATE = 48000  # Sampling rate
    AMPLITUDE_THRESHOLD = 300  # Threshold for detecting sound
    SILENCE_THRESHOLD = 5  # Seconds of silence before stopping
    CHUNK_SIZE = 4096  # Adjusted for Raspberry Pi efficiency
    CUT_SECONDS = 5  # Trim this much from the end
    CUT_SAMPLES = RATE * CUT_SECONDS  # Exact sample count to remove

    last_sound_time = None
    recording = False
    temp_file_path = None
    final_file_path = None
    audio_buffer = np.array([], dtype=np.int16)  # Store full audio efficiently

    p = pyaudio.PyAudio()
    stream = p.open(format=pyaudio.paInt16, channels=1, rate=RATE, input=True, input_device_index=input_device_id, frames_per_buffer=CHUNK_SIZE)

    serial_name = open_serial_port(com_port)

    try:
        while True:
            try:
                indata = np.frombuffer(stream.read(CHUNK_SIZE, exception_on_overflow=False), dtype=np.int16)
                max_amplitude = np.max(np.abs(indata))

                if max_amplitude > AMPLITUDE_THRESHOLD:
                    if not recording:
                        print("Recording started.")
                        filename = f"{serial_name}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.wav"
                        temp_file_path = f"./cache/{filename}"
                        final_file_path = f"./recordings/{filename}"
                        os.makedirs(os.path.dirname(temp_file_path), exist_ok=True)

                        recording = True
                        last_sound_time = time.time()
                        audio_buffer = np.array([], dtype=np.int16)  # Reset buffer

                    audio_buffer = np.concatenate((audio_buffer, indata))
                    last_sound_time = time.time()

                elif recording:
                    if time.time() - last_sound_time > SILENCE_THRESHOLD:
                        print(f"Silence detected. Saving: {temp_file_path}")

                        # Ensure we don't remove too much (avoid empty file)
                        if len(audio_buffer) > CUT_SAMPLES:
                            trimmed_audio = audio_buffer[:-CUT_SAMPLES]  # Remove last 5 seconds
                        else:
                            trimmed_audio = audio_buffer  # Keep all if too short

                        if trimmed_audio.size > 0:
                            with wave.open(temp_file_path, "wb") as wf:
                                wf.setnchannels(1)
                                wf.setsampwidth(2)
                                wf.setframerate(RATE)
                                wf.writeframes(trimmed_audio.tobytes())

                            move_file_to_recordings(temp_file_path, final_file_path)
                        else:
                            print("Recording too short, skipping save.")

                        recording = False
                        last_sound_time = None
                        audio_buffer = np.array([], dtype=np.int16)  # Reset buffer

            except IOError as e:
                print(f"Audio read error: {e}")
                time.sleep(0.1)

    except KeyboardInterrupt:
        print("Recording stopped.")

    finally:
        try:
            stream.stop_stream()
            stream.close()
            print("Stream stopped successfully.")
        except Exception as e:
            print(f"Error stopping the stream: {e}")


def start_recording(input_device_id, com_port, debug):
    """Starts the recording process in a separate thread."""
    recorder_thread = threading.Thread(target=recorder, args=(input_device_id, com_port, debug), daemon=True)
    recorder_thread.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("Stopping recording.")
        recorder_thread.join()


def main():
    """Loads configuration and starts recording."""
    with open("config.json") as f:
        config = json.load(f)

    input_device_id = config["input_device"]
    com_port = open_serial_port(config["com_port"])
    debug = config.get("debug", False)

    print(f"Starting recording on device {input_device_id} with COM port {com_port}")
    start_recording(input_device_id, com_port, debug)


if __name__ == "__main__":
    main()
