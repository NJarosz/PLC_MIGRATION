#!/usr/bin/env bash
# setup.sh — Bootstrap the PLC server on the Raspberry Pi.
#
# Run once from the pi/ directory after cloning / rsyncing:
#   cd ~/plc_server && bash setup.sh
#
# To install to a custom path:
#   bash setup.sh /opt/plc_server
set -euo pipefail

INSTALL_DIR="${1:-$HOME/plc_server}"
SERVICE_NAME="plc-server"
SERVICE_USER="${SUDO_USER:-$(whoami)}"

echo "=== PLC Server Setup ==="
echo "Install dir : $INSTALL_DIR"
echo "Service user: $SERVICE_USER"
echo ""

# ── Directory tree ────────────────────────────────────────────────────────────
mkdir -p "$INSTALL_DIR"/{api/{templates,static},compiler,sequences/{definitions,compiled},plc_registry,logs}

# ── Copy server files ─────────────────────────────────────────────────────────
cp api/app.py                       "$INSTALL_DIR/api/"
cp api/templates/*.html             "$INSTALL_DIR/api/templates/"
cp api/static/style.css             "$INSTALL_DIR/api/static/"
cp compiler/compile_sequence.py     "$INSTALL_DIR/compiler/"
cp event_codes.py                   "$INSTALL_DIR/"
cp employees.json                   "$INSTALL_DIR/" 2>/dev/null || true
cp requirements.txt                 "$INSTALL_DIR/"

# Copy example sequences (don't overwrite existing definitions)
for f in sequences/definitions/*.json; do
    dest="$INSTALL_DIR/sequences/definitions/$(basename "$f")"
    [ -f "$dest" ] || cp "$f" "$dest"
done

# .env
if [ -f .env ]; then
    cp .env "$INSTALL_DIR/.env"
elif [ ! -f "$INSTALL_DIR/.env" ]; then
    cp .env.example "$INSTALL_DIR/.env"
    echo "Note: copied .env.example → .env — edit it if needed."
fi

# ── Python virtual environment ────────────────────────────────────────────────
echo "Setting up Python virtual environment..."
python3 -m venv "$INSTALL_DIR/venv"
"$INSTALL_DIR/venv/bin/pip" install --upgrade pip -q
"$INSTALL_DIR/venv/bin/pip" install -r "$INSTALL_DIR/requirements.txt" -q
echo "Dependencies installed."

# ── systemd service ───────────────────────────────────────────────────────────
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"

echo "Installing systemd service → $SERVICE_FILE"
sudo tee "$SERVICE_FILE" > /dev/null <<EOF
[Unit]
Description=PLC Manager Flask Server
After=network.target

[Service]
Type=simple
User=${SERVICE_USER}
WorkingDirectory=${INSTALL_DIR}
ExecStart=${INSTALL_DIR}/venv/bin/python3 ${INSTALL_DIR}/api/app.py
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable "$SERVICE_NAME"
sudo systemctl restart "$SERVICE_NAME"

echo ""
echo "=== Setup complete ==="
echo ""
echo "Service status:"
sudo systemctl status "$SERVICE_NAME" --no-pager -l || true
echo ""
echo "Useful commands:"
echo "  sudo systemctl restart $SERVICE_NAME   # restart after a deploy"
echo "  sudo systemctl stop    $SERVICE_NAME   # stop the server"
echo "  journalctl -u $SERVICE_NAME -f         # tail logs"
echo "  curl http://localhost:5000/health       # quick health check"
