
---

## 🚀 Installation

1. Make the installer executable:

   ```bash
   chmod +x install.sh
   ```

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
AUDIO_OUTPUT_DEVICE=0
USER_NAME=fhadz
WORKDIR=/home/fhadz/recorder
RECORDER_CMD=/home/fhadz/recorder/recorder
REPO_BRANCH=main
AMPLITUDE_THRESHOLD=300
DEBUG_AMPLITUDE=true
LIVE_LISTEN=true
EXTRA_TEXT=ez
```

Replace the values with your actual configuration.

## 🎙 Audio quality

The recorder automatically filters the audio to emphasize the voice range and
adjusts its gain as the signal gets quieter or louder. The same processing is
used for saved recordings and live listening, so you should not need to adjust
the volume for each conversation.

Automatic gain cannot repair audio that is already distorted or clipped by the
radio or audio adapter. If that happens, lower the radio's output level enough
to avoid distortion at the input; the recorder will still level the remaining
volume differences automatically.

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

---

## 🔒 System Recovery and Auto-Reboot

To make the Raspberry Pi automatically reboot on system failure, kernel panic, or if the recorder hangs:

### 1. Reboot on systemd crash

Edit `/etc/systemd/system.conf` **and** `/etc/systemd/user.conf` and add:

```ini
[Manager]
# Reboot instead of halt on failure
CrashReboot=yes
```

Then reload systemd:

```bash
sudo systemctl daemon-reexec
```

### 2. Reboot on kernel panic

Edit `/etc/sysctl.conf` (or create `/etc/sysctl.d/99-panic.conf`) and add:

```ini
kernel.panic = 5
```

This ensures the Pi reboots 5 seconds after a kernel panic. Apply immediately without reboot:

```bash
sudo sysctl -w kernel.panic=5
```

### 3. Watchdog (hardware failsafe)

If the system freezes completely, the hardware watchdog ensures a reboot:

* Install watchdog:

  ```bash
  sudo apt install watchdog
  ```
* Enable Raspberry Pi watchdog module:

  ```bash
  sudo modprobe bcm2835_wdt
  echo bcm2835_wdt | sudo tee -a /etc/modules
  ```
* Configure `/etc/watchdog.conf`:

  ```ini
  watchdog-device = /dev/watchdog
  watchdog-timeout = 10
  ```
* Enable and start the watchdog service:

  ```bash
  sudo systemctl enable watchdog
  sudo systemctl start watchdog
  ```

### 4. Recorder integration

The recorder program automatically feeds the watchdog every second.
If it freezes or crashes, the Pi will reboot after the configured timeout.

---
