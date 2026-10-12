#!/usr/bin/env python3
import socket
import os
import sys
import subprocess

# --- Config ---
PC_CONTROL_PORT = 7777

TOKEN_SHUTDOWN          = "secure_pc_shutdown_token_12345"
TOKEN_EMERGENCY_STOP    = "secure_pc_emergency_stop_token_12345"
TOKEN_EMERGENCY_RELEASE = "secure_pc_emergency_release_token_12345"
# --------------

def lock_screen():
    """Lock all active user sessions / screen."""
    print("🔒 Locking screen...")
    try:
        subprocess.Popen(["loginctl", "lock-sessions"])
    except Exception as e:
        print(f"Error calling loginctl lock-sessions: {e}")
    try:
        res = subprocess.run(["loginctl", "list-sessions", "--no-legend"], capture_output=True, text=True)
        for line in res.stdout.strip().splitlines():
            parts = line.split()
            if parts:
                subprocess.Popen(["loginctl", "lock-session", parts[0]])
    except Exception:
        pass

def unlock_screen():
    """Unlock all active user sessions / screen."""
    print("🔓 Unlocking screen...")
    try:
        subprocess.Popen(["loginctl", "unlock-sessions"])
    except Exception as e:
        print(f"Error calling loginctl unlock-sessions: {e}")
    try:
        res = subprocess.run(["loginctl", "list-sessions", "--no-legend"], capture_output=True, text=True)
        for line in res.stdout.strip().splitlines():
            parts = line.split()
            if parts:
                subprocess.Popen(["loginctl", "unlock-session", parts[0]])
    except Exception:
        pass

def handle_command(token):
    token = token.strip()
    
    if token == TOKEN_SHUTDOWN:
        print("Valid shutdown token received! Executing shutdown...")
        subprocess.Popen(["sudo", "shutdown", "-h", "now"])
        return "ACK: Shutting down\n", True

    elif token == TOKEN_EMERGENCY_STOP:
        print("⚠️ EMERGENCY STOP token received! Locking screen...")
        try:
            subprocess.Popen(["notify-send", "-u", "critical", "EMERGENCY STOP", "Emergency stop button pressed! Screen locked."])
        except Exception:
            pass
        lock_screen()
        return "ACK: Emergency stop received (screen locked)\n", False

    elif token == TOKEN_EMERGENCY_RELEASE:
        print("✅ EMERGENCY RELEASE token received. Unlocking screen...")
        unlock_screen()
        try:
            subprocess.Popen(["notify-send", "-u", "normal", "EMERGENCY RELEASE", "Emergency stop released. Screen unlocked."])
        except Exception:
            pass
        return "ACK: Emergency release received (screen unlocked)\n", False

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
