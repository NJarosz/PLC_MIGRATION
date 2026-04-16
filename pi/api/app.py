#!/usr/bin/env python3
"""
app.py — PLC Supervisor REST API

Endpoints:
    GET  /health                         - liveness check
    GET  /deployments/<plc_id>           - serve active compiled binary for a PLC
    POST /logs/<plc_id>                  - receive a JSON log batch from a PLC
    GET  /sequences                      - list compiled .bin files
"""

import json
import os
from datetime import datetime, timezone

from dotenv import load_dotenv
from flask import Flask, abort, jsonify, request, send_file

# Load .env from the server root (one level up from api/)
load_dotenv(os.path.join(os.path.dirname(__file__), "..", ".env"))

app = Flask(__name__)

# All paths are resolved relative to the server root (one level up from api/)
SERVER_ROOT  = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
REGISTRY_DIR = os.path.join(SERVER_ROOT, "plc_registry")
COMPILED_DIR = os.path.join(SERVER_ROOT, "sequences", "compiled")
LOGS_RAW_DIR = os.path.join(SERVER_ROOT, "logs", "raw")


def load_registry(plc_id: str) -> dict | None:
    path = os.path.join(REGISTRY_DIR, f"{plc_id}.json")
    if not os.path.exists(path):
        return None
    with open(path) as f:
        return json.load(f)


# ---------------------------------------------------------------------------
# Health
# ---------------------------------------------------------------------------

@app.route("/health")
def health():
    return jsonify({
        "status": "ok",
        "timestamp": datetime.now(timezone.utc).isoformat(),
    })


# ---------------------------------------------------------------------------
# Deployments  (ESP32 calls this to fetch the active sequence binary)
# ---------------------------------------------------------------------------

@app.route("/deployments/<plc_id>", methods=["GET"])
def get_deployment(plc_id: str):
    registry = load_registry(plc_id)
    if registry is None:
        abort(404, description=f"Unknown PLC: {plc_id}")

    bin_path = registry.get("active_bin")
    if not bin_path:
        abort(404, description=f"No sequence deployed for {plc_id}")

    # active_bin is stored relative to server root by compile_sequence.py
    full_path = os.path.join(SERVER_ROOT, bin_path) if not os.path.isabs(bin_path) else bin_path
    if not os.path.exists(full_path):
        abort(404, description=f"Binary not found on disk: {bin_path}")

    print(f"[Pi] Serving '{registry['active_sequence']}' v{registry['active_version']} to {plc_id}")
    return send_file(full_path, mimetype="application/octet-stream", as_attachment=False)


# ---------------------------------------------------------------------------
# Logs  (STM32 → ESP32 → Pi log upload — future)
# ---------------------------------------------------------------------------

@app.route("/logs/<plc_id>", methods=["POST"])
def receive_logs(plc_id: str):
    if load_registry(plc_id) is None:
        abort(404, description=f"Unknown PLC: {plc_id}")

    events = request.get_json(force=True, silent=True)
    if events is None:
        abort(400, description="Request body must be a JSON array of log events")

    os.makedirs(LOGS_RAW_DIR, exist_ok=True)
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    log_file  = os.path.join(LOGS_RAW_DIR, f"{plc_id}_{timestamp}.json")

    with open(log_file, "w") as f:
        json.dump({
            "plc_id":      plc_id,
            "received_at": timestamp,
            "events":      events,
        }, f, indent=2)

    count = len(events) if isinstance(events, list) else 1
    print(f"[Pi] {count} log event(s) from {plc_id} → {log_file}")
    return jsonify({"status": "ok", "events_received": count})


# ---------------------------------------------------------------------------
# Sequences  (informational — not called by firmware)
# ---------------------------------------------------------------------------

@app.route("/sequences", methods=["GET"])
def list_sequences():
    if not os.path.exists(COMPILED_DIR):
        return jsonify([])
    files = sorted(f for f in os.listdir(COMPILED_DIR) if f.endswith(".bin"))
    return jsonify(files)


# ---------------------------------------------------------------------------

if __name__ == "__main__":
    app.run(
        host=os.environ.get("FLASK_HOST", "0.0.0.0"),
        port=int(os.environ.get("FLASK_PORT", 5000)),
        debug=os.environ.get("FLASK_DEBUG", "false").lower() == "true",
    )
