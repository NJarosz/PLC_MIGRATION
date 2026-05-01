#!/usr/bin/env bash
# deploy.sh — push Pi server files to the Raspberry Pi over SSH
#
# Usage:
#   ./deploy.sh                         # uses defaults below
#   PI_HOST=192.168.1.50 ./deploy.sh    # override host
#   PI_USER=pi ./deploy.sh              # override user

set -euo pipefail

PI_HOST="${PI_HOST:-raspberrypi.local}"
PI_USER="${PI_USER:-pi}"
PI_DIR="${PI_DIR:-/home/pi/plc_server}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "[deploy] Syncing pi/ → ${PI_USER}@${PI_HOST}:${PI_DIR}"

rsync -av --progress \
    --exclude '__pycache__/' \
    --exclude '*.pyc' \
    --exclude '.env' \
    --exclude 'logs/' \
    --exclude 'sequences/compiled/' \
    --exclude 'venv/' \
    "${SCRIPT_DIR}/" \
    "${PI_USER}@${PI_HOST}:${PI_DIR}/"

echo "[deploy] Done. To restart the server on the Pi:"
echo "         ssh ${PI_USER}@${PI_HOST} 'sudo systemctl restart plc-server'"
