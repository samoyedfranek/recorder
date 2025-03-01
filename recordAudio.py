import os
import numpy as np
import wave
import json
import time
import threading
from datetime import datetime
from serialReader import open_serial_port
import pyaudio


def save_audio_chunk(wave_file, audio_frames, debug):
    """Save audio chunk to file."""
    if audio_frames.size == 0:
        return
    wave_file.writeframes(audio_frames.astype(np.int16).tobytes())
    if debug:
        print(f"Writing {len(audio_frames)} samples to file")


def recorder(input_device_id, com_port, debug):
    """Main recording logic with continuous saving."""
    RATE = 48000  # Audio sample rate
    AMPLITUDE_THRESHOLD = 200  # Threshold for detecting sound
    SILENCE_THRESHOLD = 5  # Seconds of silence before stopping recording
    CHUNK_SIZE = 1024  # Number of frames to read at once

    audio_frames = np.array([])  # Holds the audio frames for the current chunk
    last_sound_time = None
    recording = [False]  # State of recording

    # Initialize PyAudio
    p = pyaudio.PyAudio()
    stream = p.open(format=pyaudio.paInt16, channels=1, rate=RATE, input=True, input_device_index=input_device_id, frames_per_buffer=CHUNK_SIZE)

    # Prepare the output file for live saving
    serial_name = open_serial_port(com_port)
    filename = f"{serial_name}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.wav"
    file_path = f"./recordings/{filename}"
    os.makedirs(os.path.dirname(file_path), exist_ok=True)

    # Open the file for writing
    with wave.open(file_path, "wb") as wf:
        wf.setnchannels(1)  # Mono channel
        wf.setsampwidth(2)  # 2 bytes for 16-bit audio
        wf.setframerate(RATE)

        # Main loop to record audio
        try:
            while True:
                indata = np.frombuffer(stream.read(CHUNK_SIZE), dtype=np.int16)

                # Calculate max amplitude in the current chunk
                max_amplitude = np.max(np.abs(indata))

                if debug:
                    print(f"Current max amplitude: {max_amplitude}")

                # Start or continue recording if sound is detected
                if max_amplitude > AMPLITUDE_THRESHOLD:
                    last_sound_time = time.time()
                    if not recording[0]:
                        print("Recording started.")
                        audio_frames = np.array([])  # Clear audio frames when starting a new recording
                    recording[0] = True
                    audio_frames = np.concatenate((audio_frames, indata))

                # Save the audio and reset if silence is detected for the threshold period
                elif recording[0]:
                    audio_frames = np.concatenate((audio_frames, indata))

                    # If silence period is exceeded, stop recording
                    if time.time() - last_sound_time > SILENCE_THRESHOLD:
                        print(f"Silence detected. Saving audio file: {filename}")
                        save_audio_chunk(wf, audio_frames, debug)
                        audio_frames = np.array([])  # Clear audio frames after saving
                        recording[0] = False
                        last_sound_time = None

        except KeyboardInterrupt:
            print("Recording stopped.")

        finally:
            # Close the audio stream
            stream.stop_stream()
            stream.close()


def start_recording(input_device_id, com_port, debug):
    """Start the recording in a separate thread."""
    recorder_thread = threading.Thread(target=recorder, args=(input_device_id, com_port, debug), daemon=True)
    recorder_thread.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("Recording stopped.")
        recorder_thread.join()


def main():
    """Load configuration and start the recording process."""
    config = json.load(open("config.json"))
    input_device_id = config["input_device"]
    com_port = open_serial_port(config["com_port"])
    debug = config.get("debug", False)

    print(f"Starting recording on device {input_device_id} with COM port {com_port}")
    start_recording(input_device_id, com_port, debug)


if __name__ == "__main__":
    main()
