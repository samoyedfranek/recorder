import threading
import os
import time
from googleDrive import authenticate_google_drive, upload_to_google_drive
from telegramSend import send_to_telegram, send_telegram_status
from recordAudio import record
from cli import load_config, prompt_user_for_config, list_com_ports

CONFIG_FILE = 'config.json'
BOT_TOKEN = '7759050359:AAF4tx3FUZWpBkkyuIK_miihDtzfP39WoCM'
CHAT_ID = ['6088271522', '5491210881']
DIRECTORY_TO_MONITOR = "./recordings"
GOOGLE_DRIVE_FOLDER_ID = '1eEMhEHFETEi8uQec5OhrmWdF-RZfxUAy'  # Replace with your folder ID

def prioritize_google_drive_and_telegram(file_path, service, folder_id, bot_token, chat_ids):
    """First upload the file to Google Drive, then send it to Telegram."""
    try:
        print(f"Uploading {file_path} to Google Drive...")
        upload_to_google_drive(file_path, folder_id, service)
        google_drive_success = True
    except Exception as e:
        print(f"Failed to upload {file_path} to Google Drive: {e}")
        google_drive_success = False

    if google_drive_success:
        print(f"Sending {file_path} to Telegram...")
        telegram_success = send_to_telegram(file_path, bot_token, chat_ids)
        if telegram_success:
            print(f"File {file_path} sent to Telegram successfully.")
        else:
            print(f"Failed to send {file_path} to Telegram.")
    else:
        print(f"Skipping Telegram because Google Drive upload failed for {file_path}.")


def monitor_directory(directory, drive_service, folder_id, bot_token, chat_ids):
    """Monitor a directory for new files and process them."""
    processed_files = set()
    print(f"Monitoring directory: {directory} for new audio files.")

    while True:
        try:
            current_files = set(os.listdir(directory))
            new_files = current_files - processed_files

            for file_name in new_files:
                if file_name.endswith('.wav'):  # Process only WAV files
                    file_path = os.path.join(directory, file_name)
                    time.sleep(5)
                    prioritize_google_drive_and_telegram(file_path, drive_service, folder_id, bot_token, chat_ids)

            processed_files = current_files
        except Exception as e:
            print(f"Error while monitoring directory: {e}")
        time.sleep(2)


def monitor_and_record(input_device_id, drive_service, folder_id):
    """Handle monitoring and recording in parallel."""
    print(f"Using input device ID: {input_device_id}")

    try:
        # Start recording
        print("Starting recording...")
        send_telegram_status(BOT_TOKEN, CHAT_ID, "*Urządzenie gotowe do nagrywania.*")
        record_thread = threading.Thread(target=record)
        record_thread.start()

        # Start monitoring directory
        monitor_thread = threading.Thread(
            target=monitor_directory,
            args=(DIRECTORY_TO_MONITOR, drive_service, folder_id, BOT_TOKEN, CHAT_ID),
        )
        monitor_thread.start()

        # Join threads before exiting
        record_thread.join()
        monitor_thread.join()

    except Exception as e:
        print(f"An unexpected error occurred: {e}")


def main():
    # Authenticate Google Drive
    drive_service = authenticate_google_drive()
    if drive_service:
        print("Google Drive authenticated successfully.")
    else:
        print("Google Drive authentication failed. Continuing with limited functionality.")

    # Prompt user for configuration (COM port and audio input device)
    config = load_config()
    if not config:
        print("No previous configuration found. Let's configure your device.")
        config = prompt_user_for_config(list_com_ports)

    if not config:
        print("Configuration not completed, exiting.")
        return


    # Start the monitoring and recording process
    monitor_and_record(config['input_device'], drive_service, GOOGLE_DRIVE_FOLDER_ID)


if __name__ == "__main__":
    main()
