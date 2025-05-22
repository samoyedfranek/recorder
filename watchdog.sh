#!/bin/bash

# === Load config from .env ===
ENV_FILE="$(dirname "$0")/.env"
if [ -f "$ENV_FILE" ]; then
    export $(grep -v '^#' "$ENV_FILE" | xargs)
else
    echo "Missing .env file: $ENV_FILE"
    exit 1
fi

cd "$WORKDIR" || exit 1

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

        # Restart
        $RECORDER_CMD 2>/dev/null &
        RECORDER_PID=$!
        echo "[$(date)] Recorder restarted with PID $RECORDER_PID"
    fi
done