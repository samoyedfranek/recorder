import os
import numpy as np
import sounddevice as sd
from scipy.io.wavfile import write
import json
import time
import threading
from datetime import datetime
from serialReader import open_serial_port


def save_audio_file(audio_frames, file_name, rate, debug):
    if audio_frames.size == 0:
        return

    # Directly save without trimming
    file_path = f"./recordings/{file_name}.wav"
    os.makedirs(os.path.dirname(file_path), exist_ok=True)

    write(file_path, rate, audio_frames.astype(np.int16))

    if debug:
        print(f"File saved: {file_path} with {len(audio_frames)} samples")


def recorder(input_device_id, com_port, debug):
    RATE = 48000
    AMPLITUDE_THRESHOLD = 200
    SILENCE_THRESHOLD = 5

    audio_frames = np.array([])
    last_sound_time = None
    recording = [False]

    def callback(indata, frames, time_info, status):
        nonlocal last_sound_time, audio_frames

        if indata is None or len(indata) == 0:
            return

        # Compute maximum amplitude
        max_amplitude = np.max(np.abs(indata))

        # Print the current max amplitude (for debugging purposes)
        if debug:
            print(f"Current max amplitude: {max_amplitude}")

        if max_amplitude > AMPLITUDE_THRESHOLD:
            last_sound_time = time.time()
            if not recording[0]:
                print("Recording started.")
                audio_frames = np.array([])  # Clear audio frames when starting new recording
            recording[0] = True
            audio_frames = np.concatenate((audio_frames, indata.flatten()))

        elif recording[0]:
            audio_frames = np.concatenate((audio_frames, indata.flatten()))

            if time.time() - last_sound_time > SILENCE_THRESHOLD:
                # Save the audio after detecting silence
                serial_name = open_serial_port(com_port)
                filename = f"{serial_name}_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
                print(f"Silence detected. Saving audio file: {filename}")
                save_audio_file(audio_frames, filename, RATE, debug)
                audio_frames = np.array([])  # Clear audio frames after saving
                recording[0] = False
                last_sound_time = None

    with sd.InputStream(callback=callback, channels=1, samplerate=RATE, device=input_device_id, dtype="int16"):
        while True:
            time.sleep(0.05)


def start_recording(input_device_id, com_port, debug):
    reader_thread = threading.Thread(target=recorder, args=(input_device_id, com_port, debug), daemon=True)
    reader_thread.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("Recording stopped.")
        reader_thread.join()


def main():
    config = json.load(open("config.json"))
    input_device_id = config["input_device"]
    com_port = open_serial_port(config["com_port"])
    debug = config.get("debug", False)

    print(f"Starting recording on device {input_device_id} with COM port {com_port}")
    start_recording(input_device_id, com_port, debug)


if __name__ == "__main__":
    main()
