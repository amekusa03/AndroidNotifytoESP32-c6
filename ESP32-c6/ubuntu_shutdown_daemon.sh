#!/bin/bash

# --- Config ---
PORT=7777
TOKEN_SHUTDOWN="secure_pc_shutdown_token_12345"
TOKEN_EMERGENCY_STOP="secure_pc_emergency_stop_token_12345"
TOKEN_EMERGENCY_RELEASE="secure_pc_emergency_release_token_12345"
# --------------

echo "Shutdown & PC Control daemon (netcat-based) listening on port $PORT..."

while true; do
    # Receive client data using nc with a 5 second timeout
    REQUEST=$(timeout 5 nc -l -p $PORT)
    
    # Strip any whitespace/newlines
    CLEAN_REQUEST=$(echo "$REQUEST" | tr -d "\r\n[:space:]")
    
    if [ "$CLEAN_REQUEST" = "$TOKEN_SHUTDOWN" ]; then
        echo "Valid shutdown token received! Executing systemctl poweroff..."
        systemctl poweroff -i
        exit 0
    elif [ "$CLEAN_REQUEST" = "$TOKEN_EMERGENCY_STOP" ]; then
        echo "⚠️ EMERGENCY STOP token received! Locking screen..."
        loginctl lock-sessions
        notify-send -u critical "EMERGENCY STOP" "Emergency stop button pressed! Screen locked." 2>/dev/null || true
    elif [ "$CLEAN_REQUEST" = "$TOKEN_EMERGENCY_RELEASE" ]; then
        echo "✅ EMERGENCY RELEASE token received. Unlocking screen..."
        loginctl unlock-sessions
        notify-send -u normal "EMERGENCY RELEASE" "Emergency stop released. Screen unlocked." 2>/dev/null || true
    elif [ -n "$CLEAN_REQUEST" ]; then
        echo "Invalid token received: '$CLEAN_REQUEST'"
    fi
done
