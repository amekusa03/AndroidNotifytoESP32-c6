#!/usr/bin/env python3
"""
Ubuntu Desktop Notification Forwarder for ESP32-C6-GEEK (smartonoff-pc-c6+notify)

Captures desktop notifications via D-Bus and forwards rendered BGR565 images
to ESP32-C6 over TCP (default port: 5555).
"""

import sys
import os
import time
import struct
import socket
import argparse
from PIL import Image, ImageDraw, ImageFont
import dbus
import dbus.mainloop.glib
from gi.repository import GLib

# Ensure unbuffered output for systemd journald logging
sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', buffering=1)
sys.stderr = os.fdopen(sys.stderr.fileno(), 'w', buffering=1)

# Default Settings
DEFAULT_ESP32_IP = "esp32-notify.local"
DEFAULT_TCP_PORT = 5555
WIDTH = 240
HEIGHT = 135

g_esp32_ip = DEFAULT_ESP32_IP
g_tcp_port = DEFAULT_TCP_PORT


def send_image_to_esp32(img: Image.Image, ip: str, port: int) -> bool:
    """Convert PIL image to 240x135 BGR565 (for ST7789 BGR panel) and transmit via TCP socket."""
    img = img.convert('RGB')
    pixels = img.load()

    raw_data = bytearray()
    for y in range(HEIGHT):
        for x in range(WIDTH):
            r, g, b = pixels[x, y]
            # ST7789 BGR Mode: Blue in MSB (bits 15..11), Green in mid (bits 10..5), Red in LSB (bits 4..0)
            bgr565 = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3)
            raw_data.extend(struct.pack('>H', bgr565))

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(5.0)
            s.connect((ip, port))
            s.sendall(bytes(raw_data))
        print(f"✓ Notification sent to ESP32 ({len(raw_data)} bytes -> {ip}:{port})")
        return True
    except Exception as e:
        print(f"✗ Failed to send notification to ESP32 ({ip}:{port}): {e}")
        return False


def get_best_font(size: int, bold: bool = False):
    """Find the best available font supporting Japanese characters."""
    font_candidates = [
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc" if bold else "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/fonts-japanese-gothic.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" if bold else "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    ]
    for fp in font_candidates:
        if os.path.exists(fp):
            try:
                return ImageFont.truetype(fp, size)
            except IOError:
                pass
    return ImageFont.load_default()


def create_notification_image(title: str, body: str) -> Image.Image:
    """Render a clean, high-contrast dark-mode notification card (240x135)."""
    # Background: Dark Navy
    img = Image.new('RGB', (WIDTH, HEIGHT), color=(15, 23, 42))
    draw = ImageDraw.Draw(img)

    # Top Header Bar
    draw.rectangle([(0, 0), (WIDTH, 24)], fill=(30, 41, 59))
    draw.line([(0, 24), (WIDTH, 24)], fill=(51, 65, 85), width=1)

    font_header = get_best_font(13, bold=True)
    font_sub = get_best_font(12, bold=False)
    font_title = get_best_font(14, bold=True)
    font_body = get_best_font(13, bold=False)

    # Header title & Timestamp
    draw.text((8, 4), "Notification", font=font_header, fill=(56, 189, 248))
    ts = time.strftime("%H:%M")
    draw.text((WIDTH - 46, 5), ts, font=font_sub, fill=(148, 163, 184))

    # Inner Card Container
    draw.rounded_rectangle([(6, 30), (WIDTH - 6, HEIGHT - 6)], radius=4, fill=(24, 34, 53), outline=(51, 65, 85))
    # Accent indicator bar on left of card
    draw.rounded_rectangle([(6, 30), (10, HEIGHT - 6)], radius=2, fill=(56, 189, 248))

    # Title Line
    display_title = title if title else "(No Title)"
    draw.text((16, 36), display_title[:20], font=font_title, fill=(248, 250, 252))

    # Body Lines (multi-line wrap)
    if body:
        chars_per_line = 15
        lines = [body[i:i+chars_per_line] for i in range(0, min(len(body), chars_per_line * 3), chars_per_line)]
        for i, line in enumerate(lines[:3]):
            draw.text((16, 58 + i * 20), line, font=font_body, fill=(203, 213, 225))

    return img


def notification_handler(bus, message) -> None:
    """D-Bus message listener callback."""
    if message.get_member() != "Notify":
        return
    try:
        args = message.get_args_list()
        title = str(args[3]) if len(args) > 3 else "(No Title)"
        body = str(args[4]) if len(args) > 4 else ""
        print(f"Captured notification: [{title}] {body}")
        img = create_notification_image(title, body)
        send_image_to_esp32(img, g_esp32_ip, g_tcp_port)
    except Exception as e:
        print(f"Notification handling error: {e}")


def main() -> None:
    global g_esp32_ip, g_tcp_port

    parser = argparse.ArgumentParser(description="Forward Ubuntu notifications to ESP32-C6 LCD")
    parser.add_argument("--ip", default=DEFAULT_ESP32_IP, help="ESP32 IP address (default: %(default)s)")
    parser.add_argument("--port", type=int, default=DEFAULT_TCP_PORT, help="ESP32 TCP port (default: %(default)s)")
    args = parser.parse_args()

    g_esp32_ip = args.ip
    g_tcp_port = args.port

    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SessionBus()

    try:
        bus.add_match_string(
            "type='method_call',"
            "interface='org.freedesktop.Notifications',"
            "member='Notify',"
            "eavesdrop=true"
        )
        print("D-Bus filter attached (eavesdrop mode)")
    except dbus.exceptions.DBusException as e:
        print(f"eavesdrop mode fallback ({e}), using standard match")
        bus.add_match_string(
            "type='method_call',"
            "interface='org.freedesktop.Notifications',"
            "member='Notify'"
        )

    bus.add_message_filter(notification_handler)

    print(f"Notification listener active. Forwarding to ESP32 at {g_esp32_ip}:{g_tcp_port}")
    print("Test with: notify-send 'Test Title' 'Hello from Ubuntu!'")

    try:
        GLib.MainLoop().run()
    except KeyboardInterrupt:
        print("\nTerminated by user.")


if __name__ == '__main__':
    main()
