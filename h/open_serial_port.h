#ifndef OPEN_SERIAL_PORT_H
#define OPEN_SERIAL_PORT_H

// Opens the serial port with libserialport and reads data until the marker "II" is found.
// Returns a dynamically allocated string with the extracted result, or "radio" on error.
// The caller is responsible for freeing the returned string.
char *open_serial_port(const char *com_port);

#endif // OPEN_SERIAL_PORT_H
 