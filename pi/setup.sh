#!/bin/bash
# setup.sh — Bootstrap the PLC server directory structure on the Pi.
#
# Run this from the pi/ directory of the cloned repo:
#   cd PLC_MIGRATION/pi && bash setup.sh
# Or specify a custom install location:
#   bash setup.sh /opt/plc_server
set -e

INSTALL_DIR="${1:-$HOME/plc_server}"

echo "=== PLC Server Setup ==="
echo "Target: $INSTALL_DIR"
echo ""

# Create directory tree
mkdir -p "$INSTALL_DIR"/{api,compiler,sequences/{definitions,compiled},plc_registry,logs/{raw,archive}}

# Copy scripts
cp api/app.py                   "$INSTALL_DIR/api/"
cp compiler/compile_sequence.py "$INSTALL_DIR/compiler/"
cp sequences/definitions/*      "$INSTALL_DIR/sequences/definitions/"
cp plc_registry/*.json          "$INSTALL_DIR/plc_registry/"
cp requirements.txt             "$INSTALL_DIR/"

# Copy .env if present, otherwise copy the example
if [ -f .env ]; then
    cp .env "$INSTALL_DIR/.env"
else
    cp .env.example "$INSTALL_DIR/.env"
    echo "Note: copied .env.example to .env — edit it if needed."
fi

# Create a virtual environment and install dependencies inside it.
# This avoids the PEP 668 "externally-managed-environment" error on
# Raspberry Pi OS / Debian Bookworm and later.
echo "Creating Python virtual environment..."
python3 -m venv "$INSTALL_DIR/venv"
"$INSTALL_DIR/venv/bin/pip" install --upgrade pip -q
"$INSTALL_DIR/venv/bin/pip" install -r "$INSTALL_DIR/requirements.txt"

echo ""
echo "Done. Directory layout:"
echo ""
echo "  $INSTALL_DIR/"
echo "  ├── venv/                         Python virtual environment"
echo "  ├── api/app.py                    Flask REST API"
echo "  ├── compiler/compile_sequence.py  JSON → .bin compiler"
echo "  ├── sequences/"
echo "  │   ├── definitions/              JSON sequence manifests (edit these)"
echo "  │   └── compiled/                 Generated .bin blobs (do not edit)"
echo "  ├── plc_registry/                 One JSON file per PLC"
echo "  ├── .env                          Environment config (port, debug, etc.)"
echo "  └── logs/"
echo "      ├── raw/                      Raw JSON log uploads from PLCs"
echo "      └── archive/                  Future: CSV-archived logs"
echo ""
echo "Workflow:"
echo "  1. Edit sequences/definitions/example_sequence.json (or create a new one)"
echo "  2. Compile + deploy:"
echo "       cd $INSTALL_DIR"
echo "       venv/bin/python3 compiler/compile_sequence.py sequences/definitions/example_sequence.json --deploy"
echo "  3. Start API server:"
echo "       venv/bin/python3 api/app.py"
echo "  4. Press the button on the STM32 — it will request and receive the sequence."
