#!/usr/bin/env python3
import socket
import os
import sys
import subprocess
import re

# --- Config ---
PC_CONTROL_PORT = 7777

TOKEN_SHUTDOWN    = "secure_pc_shutdown_token_12345"
TOKEN_VOLUME_UP   = "secure_pc_vol_up_token_12345"
TOKEN_VOLUME_DOWN = "secure_pc_vol_down_token_12345"
TOKEN_VOLUME_MUTE = "secure_pc_vol_mute_token_12345"
TOKEN_VOLUME_GET  = "secure_pc_vol_get_token_12345"
TOKEN_VOLUME_SET  = "secure_pc_vol_set_token_12345"
# --------------

def get_pc_volume():
    try:
        # Get volume (%)
        vol_out = subprocess.check_output(["pactl", "get-sink-volume", "@DEFAULT_SINK@"]).decode("utf-8", errors="ignore")
        match = re.search(r"(\d+)%", vol_out)
        volume = int(match.group(1)) if match else 100

        # Get mute status
        mute_out = subprocess.check_output(["pactl", "get-sink-mute", "@DEFAULT_SINK@"]).decode("utf-8", errors="ignore")
        is_muted = 1 if "yes" in mute_out.lower() else 0

        return f"ACK: VOL={volume},MUTE={is_muted}\n"
    except Exception as e:
        print(f"Error getting volume: {e}, falling back to 100%")
        return "ACK: VOL=100,MUTE=0\n"

def handle_command(token):
    token = token.strip()
    
    if token == TOKEN_SHUTDOWN:
        print("Valid shutdown token received! Executing shutdown...")
        subprocess.Popen(["sudo", "shutdown", "-h", "now"])
        return "ACK: Shutting down\n", True

    elif token == TOKEN_VOLUME_UP:
        print("Volume UP command received.")
        subprocess.run(["pactl", "set-sink-volume", "@DEFAULT_SINK@", "+5%"])
        return "ACK: Volume UP\n", False

    elif token == TOKEN_VOLUME_DOWN:
        print("Volume DOWN command received.")
        subprocess.run(["pactl", "set-sink-volume", "@DEFAULT_SINK@", "-5%"])
        return "ACK: Volume DOWN\n", False

    elif token == TOKEN_VOLUME_MUTE:
        print("Volume MUTE toggle command received.")
        subprocess.run(["pactl", "set-sink-mute", "@DEFAULT_SINK@", "toggle"])
        return "ACK: Volume MUTE toggled\n", False

    elif token == TOKEN_VOLUME_GET:
        print("Volume GET command received.")
        res = get_pc_volume()
        return res, False

    elif token.startswith(TOKEN_VOLUME_SET):
        # Format: secure_pc_vol_set_token_12345:79
        parts = token.split(":")
        if len(parts) == 2 and parts[1].isdigit():
            vol_val = int(parts[1])
            if 0 <= vol_val <= 100:
                print(f"Volume SET command received: {vol_val}%")
                subprocess.run(["pactl", "set-sink-volume", "@DEFAULT_SINK@", f"{vol_val}%"])
                return f"ACK: Volume SET to {vol_val}%\n", False
        print(f"Invalid volume set format: '{token}'")
        return "ERR: Invalid volume set format\n", False

    else:
        print(f"Invalid token received: '{token}'")
        return "ERR: Invalid token\n", False

def main():
    # Bind to all interfaces on the configured port
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        server.bind(("0.0.0.0", PC_CONTROL_PORT))
        server.listen(5)
        print(f"PC Control daemon listening on port {PC_CONTROL_PORT}...")
    except Exception as e:
        print(f"Error binding to port {PC_CONTROL_PORT}: {e}")
        sys.exit(1)

    while True:
        try:
            conn, addr = server.accept()
            print(f"Connection from {addr}")
            
            # Set timeout for receiving data (5 seconds)
            conn.settimeout(5.0)
            
            # Receive up to 1024 bytes
            data = conn.recv(1024)
            received_token = data.decode("utf-8", errors="ignore")
            
            response_msg, should_stop = handle_command(received_token)
            conn.sendall(response_msg.encode("utf-8"))
            conn.close()

            if should_stop:
                break
                
        except socket.timeout:
            print("Connection timed out waiting for token.")
        except Exception as e:
            print(f"Error handling connection: {e}")

if __name__ == "__main__":
    main()
