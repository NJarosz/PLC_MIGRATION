#!/usr/bin/env python3
"""
app.py — PLC Supervisor REST API

Endpoints:
    GET  /health                         - liveness check
    GET  /deployments/<plc_id>           - serve active compiled binary for a PLC
    POST /logs/<plc_id>                  - receive a JSON log batch from a PLC
    GET  /logs/<plc_id>?date=&limit=     - query stored logs (date: YYYY-MM-DD, limit: int)
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
LOGS_DIR     = os.path.join(SERVER_ROOT, "logs")


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
    if not isinstance(events, list):
        abort(400, description="Request body must be a JSON array of log events")

    plc_log_dir = os.path.join(LOGS_DIR, plc_id)
    os.makedirs(plc_log_dir, exist_ok=True)

    today        = datetime.now(timezone.utc).strftime("%Y-%m-%d")
    received_at  = datetime.now(timezone.utc).isoformat()
    log_file     = os.path.join(plc_log_dir, f"{today}.jsonl")

    with open(log_file, "a") as f:
        for event in events:
            event["received_at"] = received_at
            f.write(json.dumps(event) + "\n")

    print(f"[Pi] {len(events)} log event(s) from {plc_id} → {log_file}")
    return jsonify({"status": "ok", "events_received": len(events)})


@app.route("/logs/<plc_id>", methods=["GET"])
def get_logs(plc_id: str):
    date  = request.args.get("date",  datetime.now(timezone.utc).strftime("%Y-%m-%d"))
    limit = int(request.args.get("limit", 100))

    log_file = os.path.join(LOGS_DIR, plc_id, f"{date}.jsonl")
    if not os.path.exists(log_file):
        return jsonify([])

    events = []
    with open(log_file) as f:
        for line in f:
            line = line.strip()
            if line:
                events.append(json.loads(line))

    return jsonify(events[-limit:])


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
