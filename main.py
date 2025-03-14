import os
import time
import json
import threading
from telegramSend import send_to_telegram, send_telegram_status
from recordAudio import recorder
from cli import load_config, prompt_user_for_config, list_com_ports

CONFIG_FILE = "config.json"
BOT_TOKEN = "7759050359:AAF4tx3FUZWpBkkyuIK_miihDtzfP39WoCM"
CHAT_ID = ["6088271522", "5491210881"]
DIRECTORY_TO_MONITOR = "./recordings"


def prioritize_telegram(file_path, bot_token, chat_ids):
    """Send the file to Telegram."""
    print(f"Sending {file_path} to Telegram...")
    telegram_success = send_to_telegram(file_path, bot_token, chat_ids)
    if telegram_success:
        print(f"File {file_path} sent to Telegram successfully.")
    else:
        print(f"Failed to send {file_path} to Telegram.")


def monitor_directory(directory, bot_token, chat_ids, stop_event):
    """Monitor a directory for new files and process them efficiently."""
    processed_files = set()
    print(f"Monitoring directory: {directory} for new audio files.")

    while not stop_event.is_set():
        try:
            current_files = set(os.listdir(directory))
            new_files = current_files - processed_files

            for file_name in new_files:
                if file_name.endswith(".wav"):  # Process only WAV files
                    file_path = os.path.join(directory, file_name)
                    time.sleep(2)  # Allow time for file writing to complete
                    prioritize_telegram(file_path, bot_token, chat_ids)

            processed_files = current_files
        except Exception as e:
            print(f"Error while monitoring directory: {e}")

        time.sleep(5)  # Reduce CPU usage by checking every 5 seconds


def monitor_and_record(input_device_id, com_port, debug):
    """Handle monitoring and recording in parallel using threading."""
    print(f"Using input device ID: {input_device_id}")

    try:
        # Send status message
        send_telegram_status(BOT_TOKEN, CHAT_ID, "*Urządzenie gotowe do nagrywania.*")

        # Start recording in a thread
        print("Listening...")
        record_thread = threading.Thread(target=recorder, args=(input_device_id, com_port, debug), daemon=True)
        record_thread.start()

        # Use threading for file monitoring to reduce CPU usage
        stop_event = threading.Event()
        monitor_thread = threading.Thread(target=monitor_directory, args=(DIRECTORY_TO_MONITOR, BOT_TOKEN, CHAT_ID, stop_event), daemon=True)
        monitor_thread.start()

        # Keep script running without blocking
        while record_thread.is_alive():
            time.sleep(1)

        # Stop the monitoring thread when recording stops
        stop_event.set()
        monitor_thread.join()

        # Ensure the recording thread terminates cleanly
        record_thread.join()

    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        send_telegram_status(BOT_TOKEN, CHAT_ID, "*Wystąpił błąd w urządzeniu, wymagane ponowne uruchomienie.*")


def main():
    """Main function to start the program."""
    config = load_config()
    if not config:
        print("No previous configuration found. Let's configure your device.")
        config = prompt_user_for_config(list_com_ports)

    if not config:
        print("Configuration not completed, exiting.")
        return

    input_device_id = config["input_device"]
    com_port = config["com_port"]
    debug = config.get("debug", False)

    # Start monitoring and recording
    monitor_and_record(input_device_id, com_port, debug)


if __name__ == "__main__":
    main()
