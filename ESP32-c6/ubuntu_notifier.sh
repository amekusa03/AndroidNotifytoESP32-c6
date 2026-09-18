#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Activate virtual environment
if [ -d "$SCRIPT_DIR/.venv" ]; then
    source "$SCRIPT_DIR/.venv/bin/activate"
elif [ -d "/home/kusa/ドキュメント/eSp32/ESP32Notify/.venv" ]; then
    source "/home/kusa/ドキュメント/eSp32/ESP32Notify/.venv/bin/activate"
elif [ -d "$HOME/.venv" ]; then
    source "$HOME/.venv/bin/activate"
fi

exec python3 "$SCRIPT_DIR/ubuntu_notifier.py" "$@"
