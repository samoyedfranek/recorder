# quansheng
BUILD: gcc -o recorder main.c open_serial_port.c recordAudio.c telegramSend.c config.c write_wav_file.c -lportaudio -lm -lserialport -lpthread -lcurl -luv -lasound -ljack
