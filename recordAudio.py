import os
import time
import wave
import json
import threading
import pyaudio
import shutil
from datetime import datetime
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
    CUT_SAMPLES = RATE * 5 * 2  # 5 seconds of audio to cut (2 bytes per sample)

    last_sound_time = None
    recording = False
    temp_file_path = None
    wf = None

    p = pyaudio.PyAudio()
    stream = p.open(format=pyaudio.paInt16, channels=1, rate=RATE, input=True, input_device_index=input_device_id, frames_per_buffer=CHUNK_SIZE)

    serial_name = open_serial_port(com_port)

    try:
        while True:
            try:
                indata = stream.read(CHUNK_SIZE, exception_on_overflow=False)

                max_amplitude = max(abs(int.from_bytes(indata[i : i + 2], "little", signed=True)) for i in range(0, len(indata), 2))

                if max_amplitude > AMPLITUDE_THRESHOLD:
                    if not recording:
                        print("Recording started.")
                        filename = f"{serial_name}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.wav"
                        temp_file_path = f"./cache/{filename}"
                        os.makedirs(os.path.dirname(temp_file_path), exist_ok=True)

                        wf = wave.open(temp_file_path, "wb")
                        wf.setnchannels(1)
                        wf.setsampwidth(2)
                        wf.setframerate(RATE)

                        recording = True
                        last_sound_time = time.time()

                    wf.writeframes(indata)  # Write chunks directly to file
                    last_sound_time = time.time()

                elif recording:
                    if time.time() - last_sound_time > SILENCE_THRESHOLD:
                        print(f"Silence detected for {SILENCE_THRESHOLD} seconds. Finalizing: {temp_file_path}")

                        # Cut the last 5 seconds before saving
                        wf._file.seek(0, 2)  # Move to the end
                        file_size = wf._file.tell()
                        if file_size > CUT_SAMPLES:
                            wf._file.truncate(file_size - CUT_SAMPLES)

                        wf.close()

                        final_file_path = f"./recordings/{filename}"
                        move_file_to_recordings(temp_file_path, final_file_path)

                        recording = False
                        last_sound_time = None

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

        if wf is not None:
            wf.close()


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
