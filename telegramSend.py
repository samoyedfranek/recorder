import os
import time
import requests
from datetime import datetime
import re

def send_to_telegram(file_path, bot_token, chat_id):
    """Send an audio file to a Telegram chat and delete it after sending."""
    url = f"https://api.telegram.org/bot{bot_token}/sendAudio"
    
    # Extract the filename
    file_name = os.path.basename(file_path)
    
    # Remove everything after and including the first underscore
    file_name_cleaned = file_name.split('_', 1)[0]  # Take the part before the first underscore
    
    # Extract date and time from the filename using regex (Assume the format is YYYYMMDD_HHMMSS)
    date_time_pattern = r"(\d{8})_(\d{6})"  # Regex to match date and time (YYYYMMDD_HHMMSS)
    match = re.search(date_time_pattern, file_name)
    
    if match:
        date_str = match.group(1)  # YYYYMMDD
        time_str = match.group(2)  # HHMMSS
        
        # Format the date from YYYYMMDD to YYYY-MM-DD
        formatted_date = datetime.strptime(date_str, "%Y%m%d").strftime("%Y-%m-%d")
        
        # Format the time from HHMMSS to HH:MM:SS
        formatted_time = datetime.strptime(time_str, "%H%M%S").strftime("%H:%M:%S")
    else:
        formatted_date = "Unknown Date"
        formatted_time = "Unknown Time"
    
    # Ensure file name has only one .wav extension
    if file_name.endswith('.wav'):
        new_file_name = f"{file_name_cleaned}.wav"
    else:
        new_file_name = f"{file_name_cleaned}.wav"
    
    new_file_path = os.path.join(os.path.dirname(file_path), new_file_name)
    
    # Rename the file (only if the name is different)
    if file_path != new_file_path:
        os.rename(file_path, new_file_path)
        print(f"File renamed to: {new_file_name}")
    
    # Construct the caption with the extracted date and time from the filename
    caption = f"*{formatted_date} {formatted_time}* - COŚ SIĘ DZIEJE"
    
    # Escape special characters for MarkdownV2
    caption_escaped = caption.replace('.', '\\.').replace('-', '\\-')
    
    try:
        with open(new_file_path, 'rb') as audio_file:
            files = {'audio': audio_file}
            data = {
                'chat_id': chat_id,
                'caption': caption_escaped,
                'parse_mode': 'MarkdownV2'
            }
            response = requests.post(url, data=data, files=files)
        
        if response.status_code == 200:
            print(f"File sent to Telegram successfully: {new_file_name}")
            
            # Wait for 5 seconds before deleting the file
            time.sleep(5)
            
            # Delete the file after sending it
            os.remove(new_file_path)
            print(f"File deleted: {new_file_name}")
        else:
            print(f"Failed to send file to Telegram: {response.status_code} - {response.text}")
    except Exception as e:
        print(f"Error sending file to Telegram: {e}")

def monitor_directory(directory, bot_token, chat_id):
    """Monitor a directory for new audio files and send them to Telegram."""
    sent_files = set()

    while True:
        try:
            # List all files in the directory with '.wav' extension
            files = {f for f in os.listdir(directory) if f.endswith('.wav')}
            
            # Find new files that haven't been sent yet
            new_files = files - sent_files
            
            for file_name in new_files:
                file_path = os.path.join(directory, file_name)
                print(f"New file detected: {file_name}")
                send_to_telegram(file_path, bot_token, chat_id)
                sent_files.add(file_name)
            
            time.sleep(5)  # Check for new files every 5 seconds
        except KeyboardInterrupt:
            print("Stopped monitoring directory.")
            break
        except Exception as e:
            print(f"Error while monitoring directory: {e}")

def start_monitoring(directory, bot_token, chat_id):
    """
    Function to start monitoring a directory for new audio files
    and send them to Telegram.
    """
    print(f"Monitoring directory: {directory} for new audio files.")
    monitor_directory(directory, bot_token, chat_id)
