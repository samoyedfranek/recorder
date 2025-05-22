#!/bin/bash

set -e

echo "🔄 Updating and installing dependencies..."

sudo apt update -y && sudo apt upgrade -y
sudo apt install -y build-essential portaudio19-dev libserialport-dev \
     libcurl4-openssl-dev libuv1-dev libasound2-dev libjack-jackd2-dev git

echo "✅ Dependencies installed."

# === Define Paths ===
SCRIPT_DIR="$(dirname "$(realpath "$0")")"
ENV_FILE="$SCRIPT_DIR/.env"
SERVICE_FILE="/etc/systemd/system/recorder.service"
WATCHDOG="$SCRIPT_DIR/watchdog.sh"

# === Generate .env if missing ===
if [ ! -f "$ENV_FILE" ]; then
    echo "⚙️ Generating .env file..."
    cat <<EOF > "$ENV_FILE"
BOT_TOKEN=your_bot_token_here
CHAT_ID=chat_id_1,chat_id_2
COM_PORT=/dev/ttyACM0
RECORDING_DIRECTORY=./recordings
AUDIO_INPUT_DEVICE=0
USER_NAME=$(whoami)
WORKDIR=$SCRIPT_DIR
RECORDER_CMD=$SCRIPT_DIR/recorder
REPO_BRANCH=main
EOF
    echo "✅ .env file created. Please update it with your actual values!"
fi

# === Load .env variables ===
export $(grep -v '^#' "$ENV_FILE" | xargs)

# === Build recorder ===
echo "🔧 Compiling recorder..."
gcc -o "$SCRIPT_DIR/recorder" main.c open_serial_port.c recordAudio.c \
    telegramSend.c config.c write_wav_file.c \
    -lportaudio -lm -lserialport -lpthread -lcurl -luv -lasound -ljack \
    | tee "$SCRIPT_DIR/recorder_build.log"
echo "✅ Compilation complete."

# === Make watchdog executable ===
if [ -f "$WATCHDOG" ]; then
    chmod +x "$WATCHDOG"
    echo "✅ Set executable permission on $WATCHDOG"
else
    echo "❌ Error: $WATCHDOG not found"
    exit 1
fi

# === Create systemd service file ===
echo "🛠 Creating systemd service at $SERVICE_FILE..."

sudo tee "$SERVICE_FILE" >/dev/null <<EOF
[Unit]
Description=Recorder Watchdog Service
After=network-online.target
Wants=network-online.target

[Service]
User=$USER_NAME
Group=audio
WorkingDirectory=$WORKDIR
ExecStart=$WATCHDOG
Restart=always
Type=simple
StandardOutput=append:$WORKDIR/watchdog.log
StandardError=append:$WORKDIR/watchdog.log
SyslogIdentifier=recorder-watchdog

[Install]
WantedBy=multi-user.target
EOF

# === Enable and start the service ===
echo "🔄 Reloading systemd..."
sudo systemctl daemon-reload

echo "✅ Enabling service..."
sudo systemctl enable recorder.service

echo "🚀 Starting service..."
sudo systemctl start recorder.service

echo ""
echo "✅ Install complete!"
echo "🔍 Check status:  sudo systemctl status recorder.service"
echo "📄 Log file:      $WORKDIR/watchdog.log"
echo "📝 .env config:   $ENV_FILE"
