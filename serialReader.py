import serial

def open_serial_port(com_port):
    try:
        # Attempt to open the serial port
        serial_port = serial.Serial(com_port, 38400, timeout=0)
        # print(f"Successfully opened serial port: {com_port}.")
        
        while True:  # Keep reading from serial port until output is found
            if serial_port.in_waiting > 0:  # Check if there is data available to read
                byte_data = serial_port.read(serial_port.in_waiting)  # Read all available bytes
                
                # Filter out non-printable characters (values outside the printable ASCII range)
                printable_data = [byte for byte in byte_data if 32 <= byte <= 126]

                # Convert filtered bytes to a string
                printable_str = ''.join([chr(byte) for byte in printable_data])
                
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
                    
                    # Return the extracted data
                    return extracted_data

    except Exception as e:
        print(f"Error opening serial port: {e}")
        return "radio"  # Return "radio" if there is an error or no serial connection
