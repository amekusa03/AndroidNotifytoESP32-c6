# SmartOn PC (Waveshare ESP32-C6-GEEK Edition)

[English](README.md) | [日本語](README.jp.md)

Firmware that turns your PC (Ubuntu / Linux / Windows) into a **Matter**-enabled smart home device using the Waveshare **ESP32-C6-GEEK** USB dongle.  
Enables remote power on (Wake-on-LAN), safe remote shutdown, and volume adjustment through voice commands and automation rules from standard smart home ecosystems such as Google Home, Google Nest Hub, Apple Home, etc.

---

## ✨ Key Features

- **Cross-Platform (Ubuntu / Windows / Linux)**: Combines USB connection detection, Wake-on-LAN, and a TCP daemon to support modern PC operating systems.
- **Wiring-Free Installation**: No motherboard header wiring, mechanical relays, or optocouplers needed. Simply plug the dongle into an available USB port on your PC.
- **Wake-on-LAN (WOL) Remote Power On**: Sends a Wake-on-LAN magic packet to the target PC MAC address over Wi-Fi upon receiving a Matter ON command.
- **Safe Shutdown & Volume Control via TCP Daemon**: Communicates with a lightweight resident daemon on the PC over TCP with authentication tokens to trigger safe shutdown and adjust system volume.
- **Matter Dimmable Plug-in Unit Support**: Implements both `OnOff` (`0x0006`) and `LevelControl` (`0x0008`, range 0–254) clusters, mapping level adjustments in Google Home to PC volume percentages (0–100%).
- **USB Connection-Based PC State Monitoring**: Polls the USB Serial/JTAG connection state (`usb_serial_jtag_is_connected()`) every 2 seconds to accurately determine the actual PC power state (`ON` / `OFF` / `BOOTING`).
- **Automatic Matter State Synchronization & Fail-Safe Guard**: Automatically synchronizes Matter attributes when state divergence is detected, protected with recursion guards (`s_syncing_attribute`) to prevent unintended command loops.
- **ST7789 Color LCD Real-Time Display**: 1.14-inch graphical display showing Wi-Fi status, assigned IP address, PC power state, operation logs, and volume changes in real-time.
- **Power Saving & Display Protection**:
  - Automatically turns off LCD backlight after the PC has been `OFF` for 5 minutes. Automatically wakes up when the PC powers on. Manual LCD toggle via short press of the BOOT button (GPIO 9).
  - Suspends per-second screen refresh and blinking animations when the PC is `OFF` to eliminate display flicker.
  - ESP-IDF `esp_pm` power management integration (Dynamic Frequency Scaling & Auto Light Sleep).
- **Google Home Compatibility Optimizations**:
  - Deterministic UniqueID (`chip-config/unique-id`) auto-generated from MAC address for seamless Google Home pairing.
  - Dynamic Wi-Fi power save management to prevent dropped IPv6 mDNS (`_matter._tcp`) packets.

---

## 🛠️ Hardware Specifications

- **MCU Board**: Waveshare ESP32-C6-GEEK
- **Display**: 1.14-inch ST7789 Color LCD (240 × 135)
- **Form Factor / Connection**: USB Type-A Dongle (plugs directly into PC USB port)
- **Physical Wiring**: None required (powered and monitored via USB port)
- **Physical Button**: GPIO 9 (Short press: Toggle LCD backlight / Long press 3s+: Factory Reset)

---

## 💻 Software Stack

| Layer | Name | Version / Details |
|---|---|---|
| OS / SDK | ESP-IDF | v5.4.1 |
| Target MCU | ESP32-C6 | `esp32c6` |
| Smart Home Standard | esp-matter / Matter | Wi-Fi (`kOnNetwork`) |
| Device Type | Dimmable Plug-in Unit | Cluster: `0x0006` (OnOff), `0x0008` (LevelControl) |
| Power Control Method | Wake-on-LAN (WOL) / TCP Daemon | UDP Port 9 (WOL) / TCP Port 7777 (Control Daemon) |

---

## ⚙️ Network Configuration (`main/wifi_creds.h`)

For security reasons, Wi-Fi credentials, MAC addresses, and security tokens are excluded from version control (`.gitignore`).  
Create `main/wifi_creds.h` prior to building and configure it according to your network environment.

### `main/wifi_creds.h` Template

```c
#pragma once

// Wi-Fi Connection Settings
#define WIFI_SSID     "Your_WiFi_SSID"
#define WIFI_PASSWORD "Your_WiFi_Password"

// Target PC Configuration
#define PC_HOSTNAME    "your-pc-hostname"     // mDNS hostname (without .local)
#define PC_MAC_ADDRESS "XX:XX:XX:XX:XX:XX"    // Target PC MAC address for Wake-on-LAN

//#define PC_STATIC_IP "192.168.11.13"        // Optional: Skip mDNS and connect directly by IP

// PC Control Port & Authentication Tokens
#define PC_CONTROL_PORT              7777
#define PC_CONTROL_TOKEN_SHUTDOWN    "secure_pc_shutdown_token_12345"
#define PC_CONTROL_TOKEN_VOLUME_UP   "secure_pc_vol_up_token_12345"
#define PC_CONTROL_TOKEN_VOLUME_DOWN "secure_pc_vol_down_token_12345"
#define PC_CONTROL_TOKEN_VOLUME_MUTE "secure_pc_vol_mute_token_12345"
#define PC_CONTROL_TOKEN_VOLUME_GET  "secure_pc_vol_get_token_12345"
#define PC_CONTROL_TOKEN_VOLUME_SET  "secure_pc_vol_set_token_12345"
```

---

## 🚀 Build & Flash Guide

### Prerequisites

- ESP-IDF v5.4.1 and `esp-matter` environment configured
- `main/wifi_creds.h` created with your credentials

### Build & Flash

```bash
# 1. Export environment variables
source $HOME/esp/esp-idf/export.sh
source $HOME/esp/esp-matter/export.sh

# 2. Set target
idf.py set-target esp32c6

# 3. Build firmware
idf.py build

# 4. Flash and monitor
idf.py -p /dev/ttyACM0 flash monitor
```

### Full Erase (Factory Reset & Re-Pairing)

```bash
# Erase NVS and Matter fabric credentials
idf.py -p /dev/ttyACM0 erase-flash

# Re-flash firmware
idf.py -p /dev/ttyACM0 flash monitor
```

---

## 📱 Google Home Pairing Information

- **Commissioning Mode**: On-Network Commissioning (Wi-Fi)
- **Manual Pairing Code**: `34970112332`
- **Pairing QR Code Generator**: [https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT%3AY.K90AFN00KA0648G00](https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT%3AY.K90AFN00KA0648G00)
- **Factory Reset**: Long-press the onboard GPIO 9 BOOT button for 3 seconds or more.

---

## 🔄 Operation Flow

1. **Power ON (Matter -> PC)**
   - Receives ON command from Google Home / smart assistant.
   - ESP32-C6 broadcasts a Wake-on-LAN (WOL) magic packet over Wi-Fi to `PC_MAC_ADDRESS`.
   - PC boots up.

2. **Power OFF (Matter -> PC)**
   - Receives OFF command from Google Home / smart assistant.
   - ESP32-C6 establishes a TCP connection to `PC_CONTROL_PORT` on the target PC.
   - Sends `PC_CONTROL_TOKEN_SHUTDOWN`.
   - The resident PC daemon validates the token and initiates a clean system shutdown (`sudo shutdown -h now`).

3. **Volume Adjustment (Matter -> PC)**
   - Receives `LevelControl` command (0–254) from Google Home.
   - Converts the level value to volume percentage (0–100%).
   - Sends `PC_CONTROL_TOKEN_VOLUME_SET:<0-100>` over TCP.
   - The resident PC daemon executes `pactl set-sink-volume` to adjust system volume in real-time.

4. **PC State Monitoring & LCD Management (USB Status -> Matter / LCD)**
   - Polls USB Serial/JTAG connection every 2 seconds.
   - Connected ➔ `PC: ON` (LCD status updated).
   - Disconnected ➔ `PC: OFF` (LCD status updated).
   - If a state discrepancy is detected, the Matter OnOff attribute is automatically synchronized.
   - **When PC is OFF for 5 minutes, LCD backlight is turned off (brightness 0) to prevent burn-in and save power.**
   - **Screen refresh and colon animation are suspended while PC is OFF to eliminate flicker.**

---

## 🐧 Ubuntu Daemon Setup

Configure the resident daemon on Ubuntu using systemd to listen for TCP control tokens from the ESP32 for safe shutdown and volume control.

### 1. Verify Daemon Script & Permissions

Use `ubuntu_shutdown_daemon.py` located in the root of the repository.  
Ensure `PC_CONTROL_PORT` and all `TOKEN_*` values match your `wifi_creds.h`.

> [!SECURITY]
> Because authentication tokens are stored in the script, restrict file permissions so that only `root` can read/execute it.
> ```bash
> sudo chown root:root ubuntu_shutdown_daemon.py
> sudo chmod 700 ubuntu_shutdown_daemon.py
> ```

### 2. Create systemd Service File

Copy the script to `/usr/local/bin/`:

```bash
sudo cp ubuntu_shutdown_daemon.py /usr/local/bin/
sudo chown root:root /usr/local/bin/ubuntu_shutdown_daemon.py
sudo chmod 700 /usr/local/bin/ubuntu_shutdown_daemon.py
```

Create `/etc/systemd/system/pc-shutdown-daemon.service`:

```bash
sudo nano /etc/systemd/system/pc-shutdown-daemon.service
```

Add the following configuration:

```ini
[Unit]
Description=ESP32 SmartOn PC Shutdown & Control Daemon
After=network.target

[Service]
Type=simple
ExecStart=/usr/bin/python3 /usr/local/bin/ubuntu_shutdown_daemon.py
Restart=always
RestartSec=5
User=root

[Install]
WantedBy=multi-user.target
```

### 3. Enable and Start Service

```bash
# 1. Reload systemd daemon
sudo systemctl daemon-reload

# 2. Enable service on system startup
sudo systemctl enable pc-shutdown-daemon.service

# 3. Start service
sudo systemctl start pc-shutdown-daemon.service
```

### 4. Status and Log Verification

```bash
# Check service status (should be "Active: active (running)")
sudo systemctl status pc-shutdown-daemon.service

# View real-time logs
sudo journalctl -u pc-shutdown-daemon.service -f
```

---

---

## 📢 LCD Notification Protocol Specification (TCP Port 5555)

You can send arbitrary notification cards to the 1.14" LCD (240×135) of the ESP32-C6-GEEK from any external PC program (Python, C#, Node.js, Go, Rust, etc.) over TCP socket.

### 1. Protocol Overview

| Item | Value / Specification |
|---|---|
| **Protocol** | TCP (Socket) |
| **Default Port** | `5555` (`NOTIFY_TCP_PORT`) |
| **Screen Resolution** | **240 × 135 pixels** (Landscape, 16:9) |
| **Color Format** | **RGB565** (16 bits / 2 bytes per pixel: R 5-bit, G 6-bit, B 5-bit) |
| **Byte Order** | Little-Endian (`[Low Byte, High Byte]`) |
| **Total Packet Size** | **64,804 bytes** (4-byte NT header + 64,800 bytes pixel data) <br> *(Headerless 64,800 bytes raw image also supported for compatibility)* |

---

### 2. Packet Structure

#### Mode A: With 'NT' Header (Recommended / 64,804 bytes total)

Allows custom notification display duration (seconds).

```
+---------------+---------------+------------------+------------------+--------------------------------------+
| Byte 0        | Byte 1        | Byte 2           | Byte 3           | Bytes 4 ~ 64803 (64,800 bytes)       |
| 'N' (0x4E)    | 'T' (0x54)    | Duration Hi (MSB)| Duration Lo (LSB)| 240x135 RGB565 Pixel Data            |
+---------------+---------------+------------------+------------------+--------------------------------------+
```

- **Magic Bytes**: `'N'` (`0x4E`), `'T'` (`0x54`)
- **Duration**: 16-bit Big-Endian unsigned integer in seconds (`1` to `600`).
  - e.g., 10 seconds ➔ `0x00, 0x0A`
  - e.g., 30 seconds ➔ `0x00, 0x1E`
  - *If 0 or out of range, default 10 seconds is used.*
- **Pixel Data**: 240 × 135 = 32,400 pixels × 2 bytes = **64,800 bytes**. Row-major order starting from `(0,0)` (top-left) to `(239,134)` (bottom-right).

#### Mode B: Raw Image (Headerless / 64,800 bytes)

Send raw 64,800 bytes of RGB565 data directly. Displays with default timeout (10 seconds).

---

### 3. Python Implementation Example

```python
import socket
import struct
from PIL import Image, ImageDraw

ESP32_IP = "192.168.11.100"  # ESP32 IP address
PORT = 5555
WIDTH, HEIGHT = 240, 135
DURATION_SEC = 10  # Display time in seconds

# 1. Render a 240x135 dark-mode card using Pillow
img = Image.new("RGB", (WIDTH, HEIGHT), color=(15, 23, 42))
draw = ImageDraw.Draw(img)
draw.rectangle([(0, 0), (WIDTH, 24)], fill=(30, 41, 59))
draw.text((10, 5), "System Alert", fill=(56, 189, 248))
draw.rounded_rectangle([(6, 30), (WIDTH - 6, HEIGHT - 6)], radius=4, fill=(24, 34, 53), outline=(51, 65, 85))
draw.text((16, 40), "Meeting in 5 mins!", fill=(255, 255, 255))
draw.text((16, 65), "Zoom: Weekly Standup", fill=(203, 213, 225))

# 2. Convert to RGB565 (Little-Endian)
pixels = img.load()
pixel_bytes = bytearray()
for y in range(HEIGHT):
    for x in range(WIDTH):
        r, g, b = pixels[x, y]
        rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        pixel_bytes.extend(struct.pack("<H", rgb565))  # 16-bit Little-Endian

# 3. Assemble 'NT' Header + Pixel Buffer
header = struct.pack(">2sH", b"NT", DURATION_SEC)  # 'NT' + 16-bit Big-Endian duration
packet = header + bytes(pixel_bytes)

# 4. Transmit over TCP Socket
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.settimeout(5.0)
    s.connect((ESP32_IP, PORT))
    s.sendall(packet)

print("✓ Notification sent successfully!")
```

---

### 4. Display Behavior & Power Management

- **Auto Wakeup**: If the LCD backlight is powered off (sleep mode), receiving a notification automatically wakes and lights the screen.
- **Auto Dismiss**: After the duration expires, the screen automatically returns to the main clock / status screen (and restores backlight off state if it was previously off).
- **Manual Dismiss**: Pressing the BOOT button (GPIO 9) on the dongle immediately dismisses the notification and returns to the status screen.

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
