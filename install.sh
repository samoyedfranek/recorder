#!/bin/bash

# --- Locate and load .env ---
ENV_FILE="$(dirname "$0")/.env"
if [ ! -f "$ENV_FILE" ]; then
    echo "Error: .env file not found at $ENV_FILE"
    exit 1
fi

# Export variables from .env
export $(grep -v '^#' "$ENV_FILE" | xargs)

WATCHDOG="$WORKDIR/watchdog.sh"
SERVICE_FILE="/etc/systemd/system/recorder.service"

echo "Using USER_NAME=$USER_NAME"
echo "Using WORKDIR=$WORKDIR"

# Make watchdog.sh executable
if [ -f "$WATCHDOG" ]; then
    chmod +x "$WATCHDOG"
    echo "Set executable permission on $WATCHDOG"
else
    echo "Error: $WATCHDOG does not exist"
    exit 1
fi

# Create systemd service file
echo "Creating systemd service file at $SERVICE_FILE..."
cat <<EOF | sudo tee "$SERVICE_FILE" >/dev/null
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
StandardOutput=journal
StandardError=journal
SyslogIdentifier=recorder-watchdog

[Install]
WantedBy=multi-user.target
EOF

# Reload systemd, enable and start service
echo "Reloading systemd daemon..."
sudo systemctl daemon-reload

echo "Enabling recorder.service..."
sudo systemctl enable recorder.service

echo "Starting recorder.service..."
sudo systemctl start recorder.service

echo "Installation complete."
echo "Check service status with: sudo systemctl status recorder.service"
