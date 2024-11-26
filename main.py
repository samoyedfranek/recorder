import threading
import json
import pyaudio
from PyInquirer import prompt
from recordAudio import record
from telegramSend import start_monitoring, send_telegram_status

CONFIG_FILE = 'config.json'
BOT_TOKEN = '7759050359:AAF4tx3FUZWpBkkyuIK_miihDtzfP39WoCM'
CHAT_ID = ['6088271522', '5491210881']
DIRECTORY_TO_MONITOR = "./recordings"

def list_audio_input_devices():
    """List available audio input devices."""
    p = pyaudio.PyAudio()
    info = p.get_host_api_info_by_index(0)
    numdevices = info.get('deviceCount')

    input_devices = {}
    for i in range(numdevices):
        device_info = p.get_device_info_by_host_api_device_index(0, i)
        if device_info.get('maxInputChannels') > 0:
            input_devices[i] = device_info.get('name')

    p.terminate()
    return input_devices

def select_audio_input_device(saved_input_device_id=None):
    """Prompt user to select an audio input device or use the saved one."""
    input_devices = list_audio_input_devices()

    if not input_devices:
        print("No audio input devices found.")
        return None

    # If a saved ID exists, use it
    if saved_input_device_id in input_devices:
        return saved_input_device_id

    questions = [
        {
            'type': 'list',
            'name': 'input_device',
            'message': 'Select the audio input device:',
            'choices': list(input_devices.values()),
        }
    ]
    answers = prompt(questions)

    # Get the numeric ID for the selected device
    input_device_id = next(id for id, name in input_devices.items() if name == answers['input_device'])

    return input_device_id

def save_config(config):
    """Save the configuration to a JSON file."""
    with open(CONFIG_FILE, 'w', encoding='utf-8') as file:
        json.dump(config, file, indent=4, ensure_ascii=False)
def monitor_and_record(input_device_id):
    """Handle monitoring and recording in parallel."""
    # Start the Telegram monitoring in a separate thread
    monitor_thread = threading.Thread(
        target=start_monitoring, 
        args=(DIRECTORY_TO_MONITOR, BOT_TOKEN, CHAT_ID), 
        daemon=True
    )
    monitor_thread.start()

    # Initialize PyAudio
    p = pyaudio.PyAudio()

    try:
        # Check if the input device ID is valid
        input_device_info = p.get_device_info_by_index(input_device_id)
        input_device_name = input_device_info['name']
        print(f"Using input device: {input_device_name}")

        # Start recording
        print("Starting recording...")
        send_telegram_status(BOT_TOKEN, CHAT_ID, "Ready")
        record_thread = threading.Thread(target=record, args=("radio",))
        record_thread.start()

        # Join threads before exiting
        record_thread.join()
        monitor_thread.join()

    except Exception as e:
        print(f"An unexpected error occurred: {e}")

def main():
    # Load saved configuration if available
    try:
        with open(CONFIG_FILE, 'r', encoding='utf-8') as file:
            config = json.load(file)
    except (FileNotFoundError, json.JSONDecodeError):
        config = None

    # If config is not available or is invalid, prompt for selection
    if not config:
        # Select audio input device
        input_device_id = select_audio_input_device()
        if input_device_id is None:
            print("Audio input device not selected, exiting.")
            return

        # Save configuration
        config = {
            'input_device': input_device_id
        }
        save_config(config)
    else:
        input_device_id = config['input_device']

    monitor_and_record(input_device_id)

if __name__ == "__main__":
    main()
