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

    for attempt in range(5):  # Retry mechanism for up to 5 attempts
        try:
            for chat_ids in chat_id:
                with open(new_file_path, 'rb') as audio_file:
                    files = {'audio': audio_file}
                    data = {
                        'chat_id': chat_ids,
                        'caption': caption_escaped,
                        'parse_mode': 'MarkdownV2'
                    }
                    response = requests.post(url, data=data, files=files)
                
                if response.status_code == 200:
                    print(f"File sent to chat ID {chat_ids} successfully: {new_file_name}")
                    break  # Exit the retry loop if successful
                else:
                    print(f"Failed to send file to chat ID {chat_ids}: {response.status_code} - {response.text}")
            else:
                # If the inner loop didn't break, continue retrying
                time.sleep(2)  # Optional delay between retries
                continue
            break  # Break outer loop if successful
        except Exception as e:
            print(f"Attempt {attempt + 1}: Error sending file to Telegram: {e}")
            time.sleep(2)  # Optional delay before retrying
    else:
        print("Failed to send the file after 5 attempts.")
        return  # Exit if all attempts fail
    
    # Wait for 5 seconds before deleting the file
    time.sleep(5)
    
    # Delete the file after sending it
    os.remove(new_file_path)
    print(f"File deleted: {new_file_name}")

def send_telegram_status(bot_token, chat_id, message):
    """Send a status message to Telegram with retries."""
    url = f"https://api.telegram.org/bot{bot_token}/sendMessage"
    caption = f"*Status:* {message}"
    caption_escaped = caption.replace('.', '\\.').replace('-', '\\-')

    for attempt in range(5):
        try:
            for chat_ids in chat_id:
                data = {'chat_id': chat_ids, 'text': caption_escaped, 'parse_mode': 'MarkdownV2'}
                response = requests.post(url, data=data)

                if response.status_code == 200:
                    print(f"Status message sent to chat ID {chat_ids} successfully.")
                else:
                    print(f"Failed to send status message to chat ID {chat_ids}: {response.status_code} - {response.text}")
            break
        except Exception as e:
            print(f"Attempt {attempt + 1} to send status message failed: {e}")
            time.sleep(5)
    else:
        print(f"Failed to send status message after 5 attempts.")
