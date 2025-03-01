import os
import time
import json
import threading
from multiprocessing import Process
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler
from telegramSend import send_to_telegram, send_telegram_status
from recordAudio import audio_recorder
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


class DirectoryMonitor(FileSystemEventHandler):

    def __init__(self, bot_token, chat_ids):
        self.bot_token = bot_token
        self.chat_ids = chat_ids

    def on_created(self, event):
        """Called when a new file is created in the directory."""
        if event.src_path.endswith(".wav"):
            time.sleep(2) 
            prioritize_telegram(event.src_path, self.bot_token, self.chat_ids)


def monitor_directory(directory, bot_token, chat_ids):
    print(f"Monitoring directory: {directory} for new audio files.")

    event_handler = DirectoryMonitor(bot_token, chat_ids)
    observer = Observer()
    observer.schedule(event_handler, directory, recursive=False)
    observer.start()

    try:
        while True:
            time.sleep(1) 
    except KeyboardInterrupt:
        observer.stop()

    observer.join()


def monitor_and_record(input_device_id, com_port, debug):
    print(f"Using input device ID: {input_device_id}")

    try:
        send_telegram_status(BOT_TOKEN, CHAT_ID, "*Urządzenie gotowe do nagrywania.*")

        print("Starting recording...")
        record_process = Process(target=audio_recorder, args=(input_device_id, com_port, debug), daemon=True)
        record_process.start()

        monitor_thread = threading.Thread(target=monitor_directory, args=(DIRECTORY_TO_MONITOR, BOT_TOKEN, CHAT_ID), daemon=True)
        monitor_thread.start()

        record_process.join()

        monitor_thread.join()

    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        send_telegram_status(BOT_TOKEN, CHAT_ID, "*Wystąpił błąd w urządzeniu, wymagane ponowne uruchomienie.*")


def main():
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

    monitor_and_record(input_device_id, com_port, debug)


if __name__ == "__main__":
    main()
