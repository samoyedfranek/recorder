import os
import time
import requests
from datetime import datetime
import re

def send_to_telegram(file_path, bot_token, chat_ids):
    """
    Send an audio file to Telegram chats and delete it after sending.
    Returns:
        bool: True if the file was sent successfully to all chat IDs, False otherwise.
    """
    url = f"https://api.telegram.org/bot{bot_token}/sendAudio"
    file_name = os.path.basename(file_path)

    # Extract file name details
    file_name_cleaned = file_name.split('_', 1)[0]  # Take the part before the first underscore
    date_time_pattern = r"(\d{8})_(\d{6})"  # Regex to match date and time
    match = re.search(date_time_pattern, file_name)

    if match:
        date_str = match.group(1)
        time_str = match.group(2)
        formatted_date = datetime.strptime(date_str, "%Y%m%d").strftime("%Y-%m-%d")
        formatted_time = datetime.strptime(time_str, "%H%M%S").strftime("%H:%M:%S")
    else:
        formatted_date = "Unknown Date"
        formatted_time = "Unknown Time"

    new_file_name = f"{file_name_cleaned}.wav" if file_name.endswith('.wav') else f"{file_name_cleaned}.wav"
    new_file_path = os.path.join(os.path.dirname(file_path), new_file_name)
    if file_path != new_file_path:
        os.rename(file_path, new_file_path)

    caption = f"*{formatted_date} {formatted_time}* - COŚ SIĘ DZIEJE"
    caption_escaped = caption.replace('.', '\\.').replace('-', '\\-')

    success = True
    for attempt in range(5):  # Retry mechanism
        failed_ids = []
        try:
            for chat_id in chat_ids:
                with open(new_file_path, 'rb') as audio_file:
                    files = {'audio': audio_file}
                    data = {
                        'chat_id': chat_id,
                        'caption': caption_escaped,
                        'parse_mode': 'MarkdownV2'
                    }
                    response = requests.post(url, data=data, files=files)

                if response.status_code == 200:
                    print(f"File sent to chat ID {chat_id} successfully: {new_file_name}")
                else:
                    print(f"Failed to send file to chat ID {chat_id}: {response.status_code} - {response.text}")
                    failed_ids.append(chat_id)

            if not failed_ids:  # All succeeded
                break
            chat_ids = failed_ids
            print(f"Retrying failed chat IDs: {failed_ids}")
            time.sleep(2)
        except Exception as e:
            print(f"Attempt {attempt + 1}: Error sending file to Telegram: {e}")
            time.sleep(2)
            success = False

    if not failed_ids:  # Only delete if all succeeded
        os.remove(new_file_path)
        print(f"File deleted: {new_file_name}")
        success = True
    else:
        print(f"Failed to send the file to some chat IDs after 5 attempts: {failed_ids}")
        success = False

    return success

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
            print(f"Error: {e}")