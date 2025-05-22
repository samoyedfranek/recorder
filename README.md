## 🚀 Installation

1. Make the installer executable:

   ```bash
   chmod +x install.sh

2. Run the installer:

   ```bash
   ./install.sh
   ```

---

## ⚙️ Configuration

Before installing, create a `.env` file in the project directory with the following contents:

```env
BOT_TOKEN=xxxxxxxxx:xxxxxxxxxxxxxxxxxxxxxxx
CHAT_ID=xxxxxxxxxxx,xxxxxxxxxxxx,xxxxxxxx
COM_PORT=/dev/ttyACM0
RECORDING_DIRECTORY=./recordings
AUDIO_INPUT_DEVICE=0
USER_NAME=fhadz
WORKDIR=/home/fhadz/recorder
RECORDER_CMD=/home/fhadz/recorder/recorder
REPO_BRANCH=main
AMPLITUDE_THRESHOLD=300
DEBUG_AMPLITUDE=true
```

Replace the values with your actual configuration.

---

## 🛠 Service

After installation, a systemd service named `recorder.service` is created and enabled. It will:

* Run on boot
* Automatically restart on failure
* Check GitHub for new commits every 5 seconds
* Pull changes and restart the recorder on update

### Useful commands:

```bash
sudo systemctl status recorder.service     # Check service status
sudo systemctl restart recorder.service    # Restart manually
sudo journalctl -u recorder.service -f     # View live logs
```

---

## 📄 Logs

Build and runtime logs are saved to:

```
watchdog.log
```