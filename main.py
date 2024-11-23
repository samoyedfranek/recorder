import threading
import json
import serial
import pyaudio
from PyInquirer import prompt
from serialReader import open_serial_port
from recordAudio import record
from telegramSend import start_monitoring

CONFIG_FILE = 'config.json'
BOT_TOKEN = 'YOUR_BOT_TOKEN'
CHAT_ID = ['CHAT_ID_1', 'CHAT_ID_2']
DIRECTORY_TO_MONITOR = "./recordings"

def list_com_ports():
    """List available serial ports on Linux."""
    import serial.tools.list_ports
    ports = [port.device for port in serial.tools.list_ports.comports()]
    return ports

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

def select_comport():
    """Prompt user to select a serial port."""
    com_ports = list_com_ports()
    if not com_ports:
        print("No COM ports found.")
        return None
    questions = [
        {
            'type': 'list',
            'name': 'com_port',
            'message': 'Select the COM port:',
            'choices': com_ports,
        }
    ]
    answers = prompt(questions)
    return answers['com_port'] if answers else None

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

def monitor_and_record(com_port, input_device_id):
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
    input_device_name = p.get_device_info_by_index(input_device_id)['name']

    print(f"Using input device: {input_device_name}")

    try:
        # Open the serial port
        serial_port = serial.Serial(com_port, 38400, timeout=0)
        print(f"Successfully opened serial port: {com_port}, using Quansheng mode...")
        
        # Attempt to open serial port and record
        serial_connection = open_serial_port(serial_port)
        if serial_connection:
            record(serial_connection)
        else:
            print("Failed to initialize serial connection.")
    
    except serial.SerialException as e:
        print(f"Error opening serial port {com_port}: {e}, switching to normal mode...")
        record("radio")
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
        # Select COM port
        com_port = select_comport()
        if not com_port:
            print("No COM port selected, exiting.")
            return

        # Select audio input device
        input_device_id = select_audio_input_device()
        if input_device_id is None:
            print("Audio input device not selected, exiting.")
            return

        # Save configuration
        config = {
            'com_port': com_port,
            'input_device': input_device_id
        }
        save_config(config)
    else:
        com_port = config['com_port']
        input_device_id = config['input_device']

    monitor_and_record(com_port, input_device_id)

if __name__ == "__main__":
    main()
