#!/bin/bash
ENV_FILE="$(dirname "$0")/.env"

# === Load config from .env early ===
if [ -f "$ENV_FILE" ]; then
    export $(grep -v '^#' "$ENV_FILE" | xargs)
else
    echo "Missing .env file: $ENV_FILE"
    exit 1
fi

# === Locate and update AUDIO_INPUT_DEVICE based on arecord if AUTODETECT != true ===
if [ "$AUTO_DETECT" != "false" ]; then
    AUDIO_DEVICE_NAME="All-In-One-Cable"

    if arecord -l | grep -q "$AUDIO_DEVICE_NAME"; then
        CARD_ID=$(arecord -l | grep "$AUDIO_DEVICE_NAME" | sed -n 's/^card \([0-9]*\):.*/\1/p')

        if [ -n "$CARD_ID" ]; then
            echo "Detected card ID: $CARD_ID for device '$AUDIO_DEVICE_NAME'"
            if grep -q '^AUDIO_INPUT_DEVICE=' "$ENV_FILE"; then
                sed -i "s/^AUDIO_INPUT_DEVICE=.*/AUDIO_INPUT_DEVICE=$CARD_ID/" "$ENV_FILE"
            else
                echo "AUDIO_INPUT_DEVICE=$CARD_ID" >> "$ENV_FILE"
            fi
        else
            echo "Could not extract card ID"
            exit 1
        fi
    else
        echo "Audio device '$AUDIO_DEVICE_NAME' not found in arecord -l output."
        exit 1
    fi
else
    echo "AUTO_DETECT=true, skipping arecord detection."
fi

# === Detect and handle COM port ===
COM_DEVICE=$(ls /dev/ttyACM* 2>/dev/null | head -n 1)
CURRENT_COM_PORT=$(grep '^COM_PORT=' "$ENV_FILE" 2>/dev/null | cut -d'=' -f2)

if [ "$CURRENT_COM_PORT" != "false" ]; then
    if [ -n "$COM_DEVICE" ]; then
        echo "Found COM device: $COM_DEVICE"
        if grep -q '^COM_PORT=' "$ENV_FILE"; then
            sed -i "s|^COM_PORT=.*|COM_PORT=$COM_DEVICE|" "$ENV_FILE"
        else
            echo "COM_PORT=$COM_DEVICE" >> "$ENV_FILE"
        fi
    else
        echo "No /dev/ttyACM* device found, setting COM_PORT empty"
        if grep -q '^COM_PORT=' "$ENV_FILE"; then
            sed -i "s|^COM_PORT=.*|COM_PORT=|" "$ENV_FILE"
        else
            echo "COM_PORT=" >> "$ENV_FILE"
        fi
    fi
fi

cd "$WORKDIR" || { echo "Failed to cd to $WORKDIR"; exit 1; }

# === Compile the recorder program ===
echo "Compiling recorder..."
if ! gcc -o recorder main.c open_serial_port.c recordAudio.c telegramSend.c config.c write_wav_file.c getRadioImage.c \
    -lportaudio -lm -lserialport -lpthread -lcurl -luv -lasound -ljack; then
    echo "Compilation failed."
    exit 1
fi
echo "Compilation succeeded."

# === Function to get Git commit hashes ===
get_hashes() {
    git fetch origin "$REPO_BRANCH" >/dev/null 2>&1
    LOCAL_HASH=$(git rev-parse HEAD)
    REMOTE_HASH=$(git rev-parse origin/"$REPO_BRANCH")
}

# === Start recorder ===
$RECORDER_CMD 2>/dev/null &
RECORDER_PID=$!
echo "Recorder started with PID $RECORDER_PID"

# === Main loop ===
while true; do
    sleep 5
    get_hashes

    if [ "$LOCAL_HASH" != "$REMOTE_HASH" ]; then
        echo "New commit detected on $REPO_BRANCH. Pulling and restarting..."
        git pull >/dev/null 2>&1

        if kill -0 $RECORDER_PID 2>/dev/null; then
            kill $RECORDER_PID
            wait $RECORDER_PID 2>/dev/null
        fi

        echo "Recompiling recorder after git pull..."
        if ! gcc -o recorder main.c open_serial_port.c recordAudio.c telegramSend.c config.c write_wav_file.c \
            -lportaudio -lm -lserialport -lpthread -lcurl -luv -lasound -ljack; then
            echo "Compilation failed after pull."
            exit 1
        fi
        echo "Compilation succeeded after pull."

        $RECORDER_CMD 2>/dev/null &
        RECORDER_PID=$!
        echo "Recorder restarted with PID $RECORDER_PID"
    fi
done
