import serialReader

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
                print(f"Received printable data: {printable_str}")
                
                # Define the start marker
                start_marker = "II"
                # Find the start index for "II"
                start_idx = printable_str.find(start_marker)
                if start_idx != -1:
                    # Extract the data after the "II" start marker
                    extracted_data = printable_str[start_idx + len(start_marker):]

                    # Remove the last occurrence of "AML", "L", or "AM" from the extracted data using endswith
                    if extracted_data.endswith("AML"):
                        extracted_data = extracted_data[:-3]  # Remove "AML"
                    elif extracted_data.endswith("L"):
                        extracted_data = extracted_data[:-1]  # Remove "L"
                    elif extracted_data.endswith("AM"):
                        extracted_data = extracted_data[:-2]  # Remove "AM"
                    
                    # Return the first extracted result and exit
                    return extracted_data

    except Exception as e:
        print(f"Error opening serial port: {e}")
        return None

if __name__ == "__main__":
    # Open the serial port (this would be done in the calling file)
    COM_PORT = "COM10"  # Adjust if needed
    serial_port = serialReader.Serial(COM_PORT, 38400, timeout=0)
    
    # Call the function with the serial port
    extracted_data = open_serial_port(serial_port)
    print(f"Extracted data: {extracted_data}")
