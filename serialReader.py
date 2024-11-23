import serial
import json
import time

# Function to open the serial port and extract data
def open_serial_port(serial_port):
    try:
        while True:  # Keep reading from serial port until output is found
            if serial_port.in_waiting > 0:  # Check if there is data available to read
                byte_data = serial_port.read(serial_port.in_waiting)  # Read all available bytes
                
                # Filter out non-printable characters (values outside the printable ASCII range)
                printable_data = [byte for byte in byte_data if 32 <= byte <= 126]

                # Convert filtered bytes to a string
                printable_str = ''.join([chr(byte) for byte in printable_data])
                print(f"Received printable data: {printable_str}")  # Debug print

                # Define the start marker
                start_marker = "II"
                # Find the start index for "II"
                start_idx = printable_str.find(start_marker)
                if start_idx != -1:
                    # Extract the data after the "II" start marker
                    extracted_data = printable_str[start_idx + len(start_marker):]

                    # Remove the last occurrence of "AML", "L", or "AM" from the extracted data
                    if extracted_data.endswith("AML"):
                        extracted_data = extracted_data[:-3]  # Remove "AML"
                    elif extracted_data.endswith("L"):
                        extracted_data = extracted_data[:-1]  # Remove "L"
                    elif extracted_data.endswith("AM"):
                        extracted_data = extracted_data[:-2]  # Remove "AM"
                    
                    print(f"Extracted data: {extracted_data}")  # Debug print
                    
                    # Return the extracted data
                    return extracted_data

            else:
                print("Waiting for data...")  # Debug print when no data is available
                time.sleep(0.1)  # Prevent the loop from overwhelming the CPU

    except Exception as e:
        print(f"Error opening serial port: {e}")
        return None


if __name__ == "__main__":
    def load_config():
        with open('config.json', 'r') as f:
            return json.load(f)

    # Load config
    config = load_config()
    COM_PORT = config["com_port"]  # Adjust as necessary

    # Open the serial port (Ensure the COM port is valid and accessible)
    try:
        serial_port = serial.Serial(COM_PORT, 38400, timeout=0)
        print(f"Successfully opened serial port: {COM_PORT}")
    except serial.SerialException as e:
        print(f"Failed to open serial port {COM_PORT}: {e}")
        exit(1)

    # Call the function with the serial port
    extracted_data = open_serial_port(serial_port)
    if extracted_data:
        print(f"Final extracted data: {extracted_data}")
    else:
        print("No data extracted.")
