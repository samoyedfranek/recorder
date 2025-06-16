#!/bin/bash

# === Locate and update AUDIO_OUTPUT_DEVICE based on arecord ===
AUDIO_DEVICE_NAME="All-In-One-Cable"
ENV_FILE="$(dirname "$0")/.env"

if arecord -l | grep -q "$AUDIO_DEVICE_NAME"; then
    CARD_LINE=$(arecord -l | grep -B 1 "$AUDIO_DEVICE_NAME" | head -n 1)
    CARD_ID=$(echo "$CARD_LINE" | sed -n 's/card \([0-9]\+\):.*/\1/p')

    if [ -n "$CARD_ID" ]; then
        echo "Detected card ID: $CARD_ID for device '$AUDIO_DEVICE_NAME'"
        # Update .env AUDIO_OUTPUT_DEVICE
        sed -i "s/^AUDIO_OUTPUT_DEVICE=.*/AUDIO_OUTPUT_DEVICE=$CARD_ID/" "$ENV_FILE"
    else
        echo "Could not extract card ID from line: $CARD_LINE"
        exit 1
    fi
else
    echo "Audio device '$AUDIO_DEVICE_NAME' not found in arecord -l output."
    exit 1
fi

# === Load config from .env ===
if [ -f "$ENV_FILE" ]; then
    export $(grep -v '^#' "$ENV_FILE" | xargs)
else
    echo "Missing .env file: $ENV_FILE"
    exit 1
fi

cd "$WORKDIR" || { echo "Failed to cd to $WORKDIR"; exit 1; }

LOGFILE="$WORKDIR/watchdog.log"

# === Compile the recorder program ===
echo "[$(date)] Compiling recorder..." | tee -a "$LOGFILE"
if ! gcc -o recorder main.c open_serial_port.c recordAudio.c telegramSend.c config.c write_wav_file.c \
    -lportaudio -lm -lserialport -lpthread -lcurl -luv -lasound -ljack 2>&1 | tee -a "$LOGFILE"; then
    echo "[$(date)] Compilation failed. See $LOGFILE for details." | tee -a "$LOGFILE"
    exit 1
fi
echo "[$(date)] Compilation succeeded." | tee -a "$LOGFILE"

# === Function to get Git commit hashes ===
get_hashes() {
    git fetch origin "$REPO_BRANCH" >/dev/null 2>&1
    LOCAL_HASH=$(git rev-parse HEAD)
    REMOTE_HASH=$(git rev-parse origin/"$REPO_BRANCH")
}

# === Start recorder ===
$RECORDER_CMD 2>/dev/null &
RECORDER_PID=$!
echo "[$(date)] Recorder started with PID $RECORDER_PID"

# === Main loop ===
while true; do
    sleep 5
    get_hashes

    if [ "$LOCAL_HASH" != "$REMOTE_HASH" ]; then
        echo "[$(date)] New commit detected on $REPO_BRANCH. Pulling and restarting..."

        git pull >/dev/null 2>&1

        # Stop the recorder if still running
        if kill -0 $RECORDER_PID 2>/dev/null; then
            kill $RECORDER_PID
            wait $RECORDER_PID 2>/dev/null
        fi

        # Re-compile after pulling new code
        echo "[$(date)] Recompiling recorder after git pull..." | tee -a "$LOGFILE"
        if ! gcc -o recorder main.c open_serial_port.c recordAudio.c telegramSend.c config.c write_wav_file.c \
            -lportaudio -lm -lserialport -lpthread -lcurl -luv -lasound -ljack 2>&1 | tee -a "$LOGFILE"; then
            echo "[$(date)] Compilation failed after pull. See $LOGFILE for details." | tee -a "$LOGFILE"
            exit 1
        fi
        echo "[$(date)] Compilation succeeded after pull." | tee -a "$LOGFILE"

        # Restart
        $RECORDER_CMD 2>/dev/null &
        RECORDER_PID=$!
        echo "[$(date)] Recorder restarted with PID $RECORDER_PID"
    fi
done
