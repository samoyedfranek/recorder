import os
import wave
import json
import time
import sounddevice as sd
import numpy as np
import threading
from datetime import datetime
from serialReader import open_serial_port

# Function to trim the audio data
def trim_audio(audio_frames, trim_seconds, rate):
    trim_samples = trim_seconds * rate * 2  # 2 bytes per sample (16-bit audio)
    total_samples = len(audio_frames) // 2

    if total_samples <= trim_samples:
        return np.array([])

    keep_samples = total_samples - trim_samples
    return audio_frames[:keep_samples * 2]

# Function to save the audio data to a file
def save_audio_file(audio_frames, file_name, rate, channels, debug):
    if audio_frames.size == 0:
        if debug:
            print("No audio data to save. Skipping file.")
        return

    audio_frames = trim_audio(audio_frames, trim_seconds=5, rate=rate)
    file_path = f"./recordings/{file_name}.wav"
    os.makedirs(os.path.dirname(file_path), exist_ok=True)

    with wave.open(file_path, "wb") as wf:
        wf.setnchannels(channels)
        wf.setsampwidth(2)  # 2 bytes per sample (16-bit audio)
        wf.setframerate(rate)
        wf.writeframes(audio_frames.tobytes())

    if debug:
        print(f"File saved: {file_path} (Trimmed 5s from end)")

# Function to handle the audio stream and callback processing
def audio_reader(input_device_id, com_port, debug):
    RATE = 44100
    AMPLITUDE_THRESHOLD = 200

    audio_frames = []
    silent_chunks = [0]
    recording = [False]

    def callback(indata, frames, time, status):
        if status:
            print(status)

        # Calculate the maximum amplitude in the current audio chunk
        max_amplitude = np.max(np.abs(indata))
        
        # Debugging output
        if debug:
            print(f"Max Amplitude: {max_amplitude}, Silent Chunks: {silent_chunks[0]}, Recording: {recording[0]}")

        # Check if the amplitude exceeds the threshold
        if max_amplitude > AMPLITUDE_THRESHOLD:
            silent_chunks[0] = 0
            if not recording[0]:
                audio_frames.clear()  # Clear previous frames
            recording[0] = True
            audio_frames.extend(indata)

        # If recording and the amplitude falls below the threshold
        elif recording[0]:
            silent_chunks[0] += 1
            audio_frames.extend(indata)
            if silent_chunks[0] >= 100:  # Threshold for silence
                # Generate filename using com_port and timestamp
                filename = f"{com_port}_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
                if debug:
                    print(f"Saving audio file: {filename}")
                save_audio_file(np.array(audio_frames), filename, RATE, 1, debug)
                audio_frames.clear()
                recording[0] = False
                silent_chunks[0] = 0

    # Start the input stream
    with sd.InputStream(callback=callback, channels=1, samplerate=RATE, device=input_device_id, dtype='int16'):
        while True:
            time.sleep(0.05)  # Reduce CPU usage

# Function to start the recording process
def start_recording(input_device_id, com_port, debug):
    # Start audio reader in a separate thread
    reader_thread = threading.Thread(target=audio_reader, args=(input_device_id, com_port, debug), daemon=True)
    reader_thread.start()

    try:
        while True:
            time.sleep(1)  # Keep the main loop alive

    except KeyboardInterrupt:
        print("Recording stopped.")
        reader_thread.join()

# Main entry point
def main():
    # Load configuration
    config = json.load(open("config.json"))
    input_device_id = config["input_device"]
    com_port = open_serial_port(config['com_port'])  # Assuming open_serial_port generates or retrieves the com_port
    debug = config.get("debug", False)

    # Start the recording process
    start_recording(input_device_id, com_port, debug)

if __name__ == "__main__":
    main()
