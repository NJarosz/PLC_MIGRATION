#!/usr/bin/env python3
"""
app.py — PLC Supervisor REST API

Endpoints:
    GET  /health                         - liveness check; includes per-PLC heartbeat status
    GET  /deployments/<plc_id>           - serve active compiled binary for a PLC
    POST /heartbeat/<plc_id>             - receive periodic heartbeat from a PLC
    POST /logs/<plc_id>                  - receive a JSON log batch from a PLC
    GET  /logs/<plc_id>?date=&limit=     - query stored logs (date: YYYY-MM-DD, limit: int)
    GET  /sequences                      - list compiled .bin files
"""

import csv
import json
import os
import sys
import time
from datetime import datetime, timezone, timedelta

from dotenv import load_dotenv
from flask import (Flask, abort, jsonify, redirect, render_template,
                   request, Response, send_file, stream_with_context, url_for)

# event_codes.py and compiler live one level up from api/
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from event_codes import EVENT_NAMES, TIER_NAMES, PRODUCTION_EVENT_TYPES, REBOOT_EVENTS
from compiler.compile_sequence import compile_from_dict

# Load .env from the server root (one level up from api/)
load_dotenv(os.path.join(os.path.dirname(__file__), "..", ".env"))

app = Flask(__name__)

# All paths are resolved relative to the server root (one level up from api/)
SERVER_ROOT     = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
REGISTRY_DIR    = os.path.join(SERVER_ROOT, "plc_registry")
COMPILED_DIR    = os.path.join(SERVER_ROOT, "sequences", "compiled")
DEFINITIONS_DIR = os.path.join(SERVER_ROOT, "sequences", "definitions")
LOGS_DIR        = os.path.join(SERVER_ROOT, "logs")
EMPLOYEES_FILE  = os.path.join(SERVER_ROOT, "employees.json")


def load_registry(plc_id: str) -> dict | None:
    path = os.path.join(REGISTRY_DIR, f"{plc_id}.json")
    if not os.path.exists(path):
        return None
    try:
        with open(path) as f:
            return json.load(f)
    except (json.JSONDecodeError, OSError):
        return None


def save_registry(plc_id: str, registry: dict) -> None:
    # Write to a temp file then atomically rename so a power cut never leaves
    # the registry blank (open("w") truncates before writing).
    path     = os.path.join(REGISTRY_DIR, f"{plc_id}.json")
    tmp_path = path + ".tmp"
    with open(tmp_path, "w") as f:
        json.dump(registry, f, indent=2)
    os.replace(tmp_path, path)


def find_definition(seq_name: str) -> dict:
    """Return the definition JSON whose sequence_id matches seq_name, or {}."""
    if not os.path.exists(DEFINITIONS_DIR):
        return {}
    for fname in os.listdir(DEFINITIONS_DIR):
        if not fname.endswith(".json"):
            continue
        try:
            with open(os.path.join(DEFINITIONS_DIR, fname)) as f:
                d = json.load(f)
            if d.get("sequence_id") == seq_name:
                return d
        except (json.JSONDecodeError, OSError):
            continue
    return {}


def load_employees() -> dict:
    """Return the employees dict {str(id): {name, role}} or {} if file is missing."""
    if not os.path.exists(EMPLOYEES_FILE):
        return {}
    try:
        with open(EMPLOYEES_FILE) as f:
            return json.load(f)
    except (json.JSONDecodeError, OSError):
        return {}


def _default_rules() -> dict:
    return {
        "max_concurrent_outputs": 4,
        "max_step_duration_ms":   30_000,
        "max_total_duration_ms":  300_000,
        "max_steps":              32,
    }


def _plc_status_all() -> list:
    """Return a list of PLC status dicts, one per registry file."""
    now = datetime.now(timezone.utc)
    result = []
    if not os.path.exists(REGISTRY_DIR):
        return result
    for fname in sorted(os.listdir(REGISTRY_DIR)):
        if not fname.endswith(".json"):
            continue
        try:
            with open(os.path.join(REGISTRY_DIR, fname)) as f:
                reg = json.load(f)
        except (json.JSONDecodeError, OSError):
            continue
        last_hb = reg.get("last_heartbeat")
        if last_hb:
            age_s  = (now - datetime.fromisoformat(last_hb)).total_seconds()
            status = "online" if age_s < _STALE_THRESHOLD_S else "stale"
        else:
            age_s  = None
            status = "never_seen"
        result.append({
            "plc_id":     reg.get("plc_id", fname[:-5]),
            "status":     status,
            "state":      reg.get("plc_state", "UNKNOWN"),
            "fault":      reg.get("plc_fault", False),
            "seq":        reg.get("plc_seq"),
            "hb_age_s":   round(age_s, 1) if age_s is not None else None,
            "description": reg.get("description", ""),
            "part_num":   reg.get("plc_part_num", ""),
            "machine_id": reg.get("plc_machine_id", ""),
            "rules":      reg.get("rules", _default_rules()),
        })
    return result


def tick_to_wall_clock(tick_ms: int, calibration: dict | None) -> datetime | None:
    """Convert a PLC tick (ms uptime) to a wall-clock datetime using the calibration anchor."""
    if not calibration:
        return None
    try:
        anchor_wall = datetime.fromisoformat(calibration["pi_wall_clock"])
        anchor_tick = int(calibration["plc_tick_ms"])
        delta_ms    = tick_ms - anchor_tick
        # Large negative delta means the tick counter reset (reboot) — stale calibration
        if delta_ms < -60_000:
            return None
        return anchor_wall + timedelta(milliseconds=delta_ms)
    except (KeyError, ValueError, TypeError):
        return None


# ---------------------------------------------------------------------------
# Health
# ---------------------------------------------------------------------------

@app.route("/health")
def health():
    now = datetime.now(timezone.utc)
    plcs = []
    if os.path.exists(REGISTRY_DIR):
        for fname in os.listdir(REGISTRY_DIR):
            if not fname.endswith(".json"):
                continue
            with open(os.path.join(REGISTRY_DIR, fname)) as f:
                reg = json.load(f)
            last_hb = reg.get("last_heartbeat")
            if last_hb:
                age_s = (now - datetime.fromisoformat(last_hb)).total_seconds()
                status = "online" if age_s < _STALE_THRESHOLD_S else "stale"
            else:
                age_s = None
                status = "never_seen"
            plcs.append({
                "plc_id":    reg.get("plc_id", fname[:-5]),
                "status":    status,
                "state":     reg.get("plc_state"),
                "fault":     reg.get("plc_fault"),
                "seq":       reg.get("plc_seq"),
                "hb_age_s":  round(age_s, 1) if age_s is not None else None,
            })

    return jsonify({
        "status":    "ok",
        "timestamp": now.isoformat(),
        "plcs":      plcs,
    })


# ---------------------------------------------------------------------------
# Employees  (ESP32 calls this to resolve a badge number to a display name)
# ---------------------------------------------------------------------------

@app.route("/employees/<int:employee_id>", methods=["GET"])
def get_employee(employee_id: int):
    employees = load_employees()
    entry = employees.get(str(employee_id))
    if not entry:
        abort(404, description=f"Employee {employee_id} not found")
    return jsonify({"employee_id": employee_id, **entry})


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
# Heartbeat  (STM32 → ESP32 → Pi, every 5 s)
# ---------------------------------------------------------------------------

# State codes match SystemState_t enum in state_machine.h
_STATE_NAMES = {0: "BOOT", 1: "IDLE", 2: "ARMED", 3: "RUNNING", 4: "FAULT"}
_STALE_THRESHOLD_S = 30  # mark offline if no heartbeat for this many seconds


@app.route("/heartbeat/<plc_id>", methods=["POST"])
def receive_heartbeat(plc_id: str):
    registry = load_registry(plc_id)
    if registry is None:
        abort(404, description=f"Unknown PLC: {plc_id}")

    body    = request.get_json(force=True, silent=True) or {}
    now     = datetime.now(timezone.utc)
    now_iso = now.isoformat()
    seq_name = body.get("seq")

    # Refresh tick calibration anchor on every heartbeat
    if body.get("tick") is not None:
        registry["tick_calibration"] = {
            "pi_wall_clock": now_iso,
            "plc_tick_ms":   int(body["tick"]),
        }

    # When the active sequence changes, look up and cache part/machine from its definition
    if seq_name and seq_name != registry.get("plc_seq"):
        defn = find_definition(seq_name)
        registry["plc_part_num"]   = defn.get("part_num", "")
        registry["plc_machine_id"] = defn.get("machine_id", "")

    if "rules" not in registry:
        registry["rules"] = _default_rules()

    registry["last_heartbeat"] = now_iso
    registry["plc_tick_ms"]    = body.get("tick")
    registry["plc_state"]      = _STATE_NAMES.get(body.get("state"), "UNKNOWN")
    registry["plc_seq"]        = seq_name
    registry["plc_fault"]      = bool(body.get("fault", 0))

    save_registry(plc_id, registry)
    return jsonify({"status": "ok"})


# ---------------------------------------------------------------------------
# Logs  (STM32 → ESP32 → Pi log upload)
# ---------------------------------------------------------------------------

@app.route("/logs/<plc_id>", methods=["POST"])
def receive_logs(plc_id: str):
    registry = load_registry(plc_id)
    if registry is None:
        abort(404, description=f"Unknown PLC: {plc_id}")

    events = request.get_json(force=True, silent=True)
    if not isinstance(events, list):
        abort(400, description="Request body must be a JSON array of log events")

    plc_log_dir = os.path.join(LOGS_DIR, plc_id)
    os.makedirs(plc_log_dir, exist_ok=True)

    today       = datetime.now(timezone.utc).strftime("%Y-%m-%d")
    received_at = datetime.now(timezone.utc)
    log_file    = os.path.join(plc_log_dir, f"{today}.jsonl")
    prod_csv    = os.path.join(plc_log_dir, "production.csv")

    calibration = registry.get("tick_calibration")
    part_num    = registry.get("plc_part_num", "")
    machine_id  = registry.get("plc_machine_id", "")
    calibration_cleared = False
    prod_rows   = []

    employees          = load_employees()
    current_employee_id = None   # set on LOGIN, cleared on LOGOUT

    with open(log_file, "a") as jf:
        for ev in events:
            code = ev.get("event")
            tick = ev.get("ts")
            data = ev.get("data")

            # Reboot event means tick counter reset — calibration is no longer valid
            if code in REBOOT_EVENTS:
                calibration = None
                calibration_cleared = True

            # Track current operator from LOGIN / LOGOUT events
            if code == 600:   # EVENT_LOGIN  — data field is employee_id
                current_employee_id = str(data) if data is not None else None
            elif code == 602:  # EVENT_LOGOUT — clear after this row is written
                pass           # cleared below, after prod_row is appended

            wall_clock = tick_to_wall_clock(tick, calibration) if tick is not None else None
            ts_for_csv = wall_clock or received_at

            # Resolve employee name for JSONL enrichment
            employee_name = None
            if current_employee_id:
                emp = employees.get(current_employee_id, {})
                employee_name = emp.get("name")

            annotated = {
                "tick":        tick,
                "tier":        TIER_NAMES.get(ev.get("tier"), str(ev.get("tier"))),
                "event":       code,
                "event_name":  EVENT_NAMES.get(code, f"UNKNOWN_{code}"),
                "data":        data,
                "employee_id": current_employee_id,
                "employee":    employee_name,
                "wall_clock":  wall_clock.isoformat() if wall_clock else None,
                "received_at": received_at.isoformat(),
            }
            jf.write(json.dumps(annotated) + "\n")

            if code in PRODUCTION_EVENT_TYPES:
                prod_rows.append({
                    "EventType": PRODUCTION_EVENT_TYPES[code],
                    "PLC":       plc_id,
                    "Machine":   machine_id,
                    "Part":      part_num,
                    "User_ID":   current_employee_id or "",
                    "Time":      ts_for_csv.strftime("%H:%M:%S"),
                    "Date":      ts_for_csv.strftime("%Y-%m-%d"),
                })

            if code == 602:   # clear after LOGOUT row is written
                current_employee_id = None

    if prod_rows:
        write_header = not os.path.exists(prod_csv)
        with open(prod_csv, "a", newline="") as cf:
            writer = csv.DictWriter(
                cf, fieldnames=["EventType","PLC","Machine","Part","User_ID","Time","Date"]
            )
            if write_header:
                writer.writeheader()
            writer.writerows(prod_rows)

    if calibration_cleared:
        registry["tick_calibration"] = None
        save_registry(plc_id, registry)

    print(f"[Pi] {len(events)} event(s) from {plc_id} → {log_file}"
          + (f"  +{len(prod_rows)} production rows" if prod_rows else ""))
    return jsonify({"status": "ok", "events_received": len(events), "production_rows": len(prod_rows)})


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
# UI page routes
# ---------------------------------------------------------------------------

@app.route("/")
def index():
    return redirect(url_for("dashboard"))


@app.route("/dashboard")
def dashboard():
    return render_template("dashboard.html")


@app.route("/designer")
def designer():
    return render_template("designer.html")


# ---------------------------------------------------------------------------
# Server-Sent Events — pushes PLC status to dashboard every 5 s
# ---------------------------------------------------------------------------

@app.route("/events")
def sse_stream():
    def generate():
        while True:
            yield f"data: {json.dumps(_plc_status_all())}\n\n"
            time.sleep(5)
    return Response(
        stream_with_context(generate()),
        mimetype="text/event-stream",
        headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"},
    )


# ---------------------------------------------------------------------------
# PLC API
# ---------------------------------------------------------------------------

@app.route("/api/plcs", methods=["GET"])
def api_plcs():
    return jsonify(_plc_status_all())


@app.route("/api/plcs/<plc_id>", methods=["GET"])
def api_plc_detail(plc_id: str):
    registry = load_registry(plc_id)
    if registry is None:
        abort(404, description=f"Unknown PLC: {plc_id}")
    if "rules" not in registry:
        registry["rules"] = _default_rules()
    return jsonify(registry)


@app.route("/api/plcs/<plc_id>/config", methods=["POST"])
def api_plc_config(plc_id: str):
    registry = load_registry(plc_id)
    if registry is None:
        abort(404, description=f"Unknown PLC: {plc_id}")

    body = request.get_json(force=True, silent=True) or {}

    if "description" in body:
        registry["description"] = str(body["description"])

    if "rules" in body:
        current = registry.get("rules", _default_rules())
        for key in ("max_concurrent_outputs", "max_step_duration_ms",
                    "max_total_duration_ms", "max_steps"):
            if key in body["rules"]:
                current[key] = int(body["rules"][key])
        registry["rules"] = current

    save_registry(plc_id, registry)
    return jsonify({"status": "ok"})


# ---------------------------------------------------------------------------
# Sequence definition API
# ---------------------------------------------------------------------------

@app.route("/api/sequences/definitions", methods=["GET"])
def api_list_definitions():
    if not os.path.exists(DEFINITIONS_DIR):
        return jsonify([])
    result = []
    for fname in sorted(os.listdir(DEFINITIONS_DIR)):
        if not fname.endswith(".json"):
            continue
        try:
            with open(os.path.join(DEFINITIONS_DIR, fname)) as f:
                d = json.load(f)
            version  = d.get("version", 1)
            bin_name = f"{d.get('sequence_id', fname[:-5])}_v{version}.bin"
            compiled = os.path.exists(os.path.join(COMPILED_DIR, bin_name))
            result.append({
                "sequence_id": d.get("sequence_id"),
                "version":     version,
                "description": d.get("description", ""),
                "target_plc":  d.get("target_plc", ""),
                "step_count":  len(d.get("steps", [])),
                "compiled":    compiled,
            })
        except (json.JSONDecodeError, OSError):
            continue
    return jsonify(result)


@app.route("/api/sequences/definitions", methods=["POST"])
def api_save_definition():
    definition = request.get_json(force=True, silent=True)
    if not definition or "sequence_id" not in definition:
        abort(400, description="sequence_id required")

    seq_id = definition["sequence_id"].strip()
    if not seq_id:
        abort(400, description="sequence_id must not be empty")

    os.makedirs(DEFINITIONS_DIR, exist_ok=True)
    out_path = os.path.join(DEFINITIONS_DIR, f"{seq_id}.json")
    with open(out_path, "w") as f:
        json.dump(definition, f, indent=2)

    return jsonify({"status": "ok", "path": out_path})


@app.route("/api/sequences/definitions/<seq_id>", methods=["GET"])
def api_get_definition(seq_id: str):
    path = os.path.join(DEFINITIONS_DIR, f"{seq_id}.json")
    if not os.path.exists(path):
        abort(404, description=f"Definition not found: {seq_id}")
    with open(path) as f:
        return jsonify(json.load(f))


@app.route("/api/sequences/compile", methods=["POST"])
def api_compile():
    body   = request.get_json(force=True, silent=True) or {}
    seq_id = body.get("sequence_id", "").strip()
    if not seq_id:
        abort(400, description="sequence_id required")

    def_path = os.path.join(DEFINITIONS_DIR, f"{seq_id}.json")
    if not os.path.exists(def_path):
        abort(404, description=f"Definition not found: {seq_id}")

    try:
        with open(def_path) as f:
            definition = json.load(f)
        out_path, _ = compile_from_dict(definition, COMPILED_DIR)
        return jsonify({"status": "ok", "output_file": os.path.basename(out_path)})
    except (ValueError, KeyError) as e:
        return jsonify({"error": str(e)}), 400


@app.route("/api/sequences/deploy", methods=["POST"])
def api_deploy():
    body   = request.get_json(force=True, silent=True) or {}
    seq_id = body.get("sequence_id", "").strip()
    plc_id = body.get("plc_id", "").strip()
    if not seq_id or not plc_id:
        abort(400, description="sequence_id and plc_id required")

    def_path = os.path.join(DEFINITIONS_DIR, f"{seq_id}.json")
    if not os.path.exists(def_path):
        abort(404, description=f"Definition not found: {seq_id}")

    with open(def_path) as f:
        definition = json.load(f)

    version  = definition.get("version", 1)
    bin_path = os.path.join(COMPILED_DIR, f"{seq_id}_v{version}.bin")
    if not os.path.exists(bin_path):
        abort(400, description=f"Compile first — {seq_id}_v{version}.bin not found")

    registry = load_registry(plc_id)
    if registry is None:
        abort(404, description=f"Unknown PLC: {plc_id}")

    registry["active_sequence"] = seq_id
    registry["active_version"]  = version
    registry["active_bin"]      = os.path.relpath(bin_path, SERVER_ROOT)
    registry["deployed_at"]     = datetime.now(timezone.utc).isoformat()
    save_registry(plc_id, registry)

    print(f"[Pi] Deployed '{seq_id}' v{version} to {plc_id}")
    return jsonify({"status": "ok", "sequence_id": seq_id, "plc_id": plc_id, "version": version})


# ---------------------------------------------------------------------------

if __name__ == "__main__":
    app.run(
        host=os.environ.get("FLASK_HOST", "0.0.0.0"),
        port=int(os.environ.get("FLASK_PORT", 5000)),
        debug=os.environ.get("FLASK_DEBUG", "false").lower() == "true",
        threaded=True,
    )
