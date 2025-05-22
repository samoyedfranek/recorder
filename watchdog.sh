#!/bin/bash
ENV_FILE="$WORKDIR/.env"

# Load environment variables from .env
if [ -f "$ENV_FILE" ]; then
    export $(grep -v '^#' "$ENV_FILE" | xargs)
fi

cd "$WORKDIR" || exit 1

# Function to get commit hashes
get_hashes() {
    git fetch origin "$REPO_BRANCH" >/dev/null 2>&1
    LOCAL_HASH=$(git rev-parse HEAD)
    REMOTE_HASH=$(git rev-parse origin/"$REPO_BRANCH")
}

# Start recorder initially
$RECORDER_CMD 2>/dev/null &
RECORDER_PID=$!

echo "[$(date)] Recorder started with PID $RECORDER_PID"

# Loop to check GitHub every 5 seconds
while true; do
    sleep 5
    get_hashes
    if [ "$LOCAL_HASH" != "$REMOTE_HASH" ]; then
        echo "[$(date)] New commit detected. Pulling changes and restarting recorder..."
        git pull >/dev/null 2>&1

        # Kill old recorder
        kill $RECORDER_PID
        wait $RECORDER_PID 2>/dev/null

        # Restart recorder
        $RECORDER_CMD 2>/dev/null &
        RECORDER_PID=$!
        echo "[$(date)] Recorder restarted with PID $RECORDER_PID"
    fi
done
