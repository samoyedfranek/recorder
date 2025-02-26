import json
import pyaudio
import serial.tools.list_ports
from PyInquirer import prompt

CONFIG_FILE = 'config.json'

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

def list_com_ports():
    """List available COM ports."""
    return [port.device for port in serial.tools.list_ports.comports()]

def select_comport(list_com_ports):
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

    input_device_id = next(
        id for id, name in input_devices.items() if name == answers['input_device']
    )

    return input_device_id

def load_config():
    """Load saved configuration from a file."""
    try:
        with open(CONFIG_FILE, 'r', encoding='utf-8') as file:
            return json.load(file)
    except (FileNotFoundError, json.JSONDecodeError):
        return None

def save_config(config):
    """Save configuration to a file."""
    with open(CONFIG_FILE, 'w', encoding='utf-8') as file:
        json.dump(config, file, indent=4, ensure_ascii=False)

def prompt_user_for_config(list_com_ports):
    """Prompt the user to select configuration options."""
    com_port = select_comport(list_com_ports)
    if not com_port:
        print("No COM port selected, exiting.")
        return None

    input_device_id = select_audio_input_device()
    if input_device_id is None:
        print("Audio input device not selected, exiting.")
        return None

    config = {
        'com_port': com_port,
        'input_device': input_device_id,
    }
    save_config(config)
    return config
