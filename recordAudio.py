import os
import time
import wave
import json
import threading
import pyaudio
import shutil
from datetime import datetime
from collections import deque
from serialReader import open_serial_port


def move_file_to_recordings(temp_file_path, final_file_path):
    """Moves the final WAV file to the recordings directory."""
    shutil.move(temp_file_path, final_file_path)
    print(f"File moved to: {final_file_path}")


def recorder(input_device_id, com_port, debug):
    RATE = 48000  # Sample rate
    AMPLITUDE_THRESHOLD = 300  # Sound detection threshold
    SILENCE_THRESHOLD = 5  # Silence duration to stop recording (seconds)
    CHUNK_SIZE = 4096  # Larger buffer size to reduce CPU load
    CUT_SECONDS = 5  # Duration to cut before saving
    CUT_CHUNKS = (RATE * CUT_SECONDS) // CHUNK_SIZE  # Chunks to remove from the end

    last_sound_time = None
    recording = False
    temp_file_path = None
    final_file_path = None
    audio_buffer = deque()  # Stores full audio for writing

    p = pyaudio.PyAudio()
    stream = p.open(format=pyaudio.paInt16, channels=1, rate=RATE, input=True, input_device_index=input_device_id, frames_per_buffer=CHUNK_SIZE)

    serial_name = open_serial_port(com_port)

    try:
        while True:
            try:
                indata = stream.read(CHUNK_SIZE, exception_on_overflow=False)

                # Calculate peak amplitude
                max_amplitude = max(abs(int.from_bytes(indata[i : i + 2], "little", signed=True)) for i in range(0, len(indata), 2))

                if max_amplitude > AMPLITUDE_THRESHOLD:
                    if not recording:
                        print("Recording started.")
                        filename = f"{serial_name}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.wav"
                        temp_file_path = f"./cache/{filename}"
                        final_file_path = f"./recordings/{filename}"
                        os.makedirs(os.path.dirname(temp_file_path), exist_ok=True)

                        recording = True
                        last_sound_time = time.time()
                        audio_buffer.clear()  # Reset buffer

                    audio_buffer.append(indata)  # Store full recording
                    last_sound_time = time.time()

                elif recording:
                    if time.time() - last_sound_time > SILENCE_THRESHOLD:
                        print(f"Silence detected for {SILENCE_THRESHOLD} seconds. Saving: {temp_file_path}")

                        # Ensure we don't remove too much (avoid empty file)
                        num_chunks = len(audio_buffer)
                        if num_chunks > CUT_CHUNKS:
                            trimmed_audio = list(audio_buffer)[:-CUT_CHUNKS]  # Remove last 5 seconds
                        else:
                            trimmed_audio = list(audio_buffer)  # Keep everything if too short

                        if trimmed_audio:
                            with wave.open(temp_file_path, "wb") as wf:
                                wf.setnchannels(1)
                                wf.setsampwidth(2)
                                wf.setframerate(RATE)

                                for chunk in trimmed_audio:
                                    wf.writeframes(chunk)

                            move_file_to_recordings(temp_file_path, final_file_path)
                        else:
                            print("Recording too short, skipping save.")

                        recording = False
                        last_sound_time = None
                        audio_buffer.clear()

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
    """Starts the recording in a separate thread."""
    recorder_thread = threading.Thread(target=recorder, args=(input_device_id, com_port, debug), daemon=True)
    recorder_thread.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("Recording stopped.")
        recorder_thread.join()


def main():
    """Main function to load config and start recording."""
    config = json.load(open("config.json"))
    input_device_id = config["input_device"]
    com_port = open_serial_port(config["com_port"])
    debug = config.get("debug", False)

    print(f"Starting recording on device {input_device_id} with COM port {com_port}")
    start_recording(input_device_id, com_port, debug)


if __name__ == "__main__":
    main()
