# PLC Supervisor — System Overview

## Pi Server

| Item | Value |
|------|-------|
| Hostname | `devpi` |
| IP address | `10.0.0.67` |
| Port | `5000` |
| Install path | `~/plc_server/` |
| systemd service | `plc-server` (auto-starts on boot) |

**Browser URLs:**
- Dashboard: `http://10.0.0.67:5000/dashboard`
- Sequence Designer: `http://10.0.0.67:5000/designer`
- Health check: `http://10.0.0.67:5000/health`

---

## Full System Architecture

```
Browser (10.0.0.67:5000)
        │
  Flask app (~/plc_server/api/app.py)
        │  REST + SSE
        │
  ESP32 (WiFi bridge)
        │  UART (PA9/PA10)
        │
  STM32F446RE
        ├── State machine / sequence engine / safety
        ├── Relay outputs (8x, GPIOA)
        └── HMI board (20-pin ribbon)
              ├── LCD 16×2 (I2C PB8/PB9)
              ├── RFID RC522 (SPI2)
              ├── Buttons: RUN PB0, FETCH PB1, ACK PB10
              ├── Hand buttons: L PB14, R PB15, 3 PC11
              └── LEDs: Armed PB5, Run PB7, Fault PB6
```

---

## Data Flows

| Event | Direction | Protocol |
|-------|-----------|----------|
| Heartbeat (every 5 s) | STM32 → ESP32 → Pi | UART text → HTTP POST `/heartbeat/<plc_id>` |
| RFID badge scan | STM32 → Pi | `LOOKUP_EMPLOYEE\|<id>` → `GET /employees/<id>` → `EMPLOYEE_NAME\|<name>` |
| Fetch sequence | STM32 → Pi | `REQUEST_SEQUENCE` → `GET /deployments/<plc_id>` → binary packet |
| Log upload | STM32 → Pi | `UPLOAD_LOGS` → `POST /logs/<plc_id>` |
| Dashboard live update | Pi → Browser | Server-Sent Events `/events` (every 5 s) |

**Heartbeat body** (STM32 format, forwarded by ESP32):
`HEARTBEAT|<tick_ms>|<state>|<seq_name>|<fault>|<count>|<boot>`

State codes: 0=BOOT 1=IDLE 2=ARMED 3=RUNNING 4=FAULT 5=GOAL_MET

---

## What Gets Logged

### JSONL Event Log
Path on Pi: `~/plc_server/logs/<plc_id>/<YYYY-MM-DD>.jsonl`

One JSON line per event. Fields:
- `tick` — MCU uptime in ms
- `wall_clock` — converted to real time via calibration anchor
- `tier` — A1 (critical), A2 (operator action), A3 (cycle), B (debug)
- `event` / `event_name` — numeric code + human name
- `data` — event-specific payload
- `employee_id` / `employee` — operator at time of event
- `received_at` — Pi wall clock when batch arrived

### Production CSV
Path on Pi: `~/plc_server/logs/<plc_id>/production.csv`

Columns: `EventType, PLC, Machine, Part, User_ID, Time, Date`

Rows written for: LOGIN, LOGOUT, SEQUENCE_START, SEQUENCE_COMPLETE, COUNT_RESET, WATCHDOG_RESET, SAFETY_RESET

### PLC Registry
Path on Pi: `~/plc_server/plc_registry/<plc_id>.json`

Live state per PLC. Updated on every heartbeat. Contains:
- `plc_state`, `plc_fault`, `plc_seq`, `plc_count`
- `active_sequence`, `active_bin`, `deployed_at`
- `plc_part_num`, `plc_machine_id`, `count_goal`
- `tick_calibration` — anchor for tick→wall-clock conversion
- `rules` — validation limits for the sequence designer

---

## Sequence Workflow

```
1. Designer UI  →  create/edit JSON definition
                   ~/plc_server/sequences/definitions/<seq_id>.json

2. Click Compile →  POST /api/sequences/compile
                    compile_sequence.py outputs binary
                    ~/plc_server/sequences/compiled/<seq_id>_v<n>.bin

3. Click Deploy  →  POST /api/sequences/deploy
                    Pi sets active_bin in PLC registry (devpi01.json)

4. Operator presses FETCH on machine
                 →  STM32 sends REQUEST_SEQUENCE
                    ESP32 → GET /deployments/<plc_id>
                    Pi serves .bin file
                    STM32 validates SHA-256, saves to flash
```

The PLC won't load a new sequence until FETCH is pressed. The dashboard shows
the sequence name the MCU last reported via heartbeat, not what's deployed.

---

## Manual Intervention (SSH)

```bash
ssh pi@devpi
# or
ssh pi@10.0.0.67
```

### Service management
```bash
sudo systemctl status plc-server       # check if running + recent logs
sudo systemctl restart plc-server      # restart after a code change
sudo systemctl stop plc-server         # stop
journalctl -u plc-server -f            # tail live logs
```

### Quick health check
```bash
curl http://localhost:5000/health
```

### View today's event log for a PLC
```bash
cat ~/plc_server/logs/devpi01/$(date +%Y-%m-%d).jsonl | python3 -m json.tool | less
```

### View production CSV
```bash
cat ~/plc_server/logs/devpi01/production.csv
```

### Manually edit PLC registry (e.g. to fix a stuck count)
```bash
nano ~/plc_server/plc_registry/devpi01.json
sudo systemctl restart plc-server
```

### Manually deploy a sequence without the UI
```bash
curl -s -X POST http://localhost:5000/api/sequences/deploy \
  -H "Content-Type: application/json" \
  -d '{"sequence_id":"seq_001","plc_id":"devpi01"}'
```

### Force count reset from CLI
```bash
curl -s -X POST http://localhost:5000/api/plcs/devpi01/reset_count
```

---

## Directory Layout (on Pi)

```
~/plc_server/
├── api/
│   ├── app.py              ← Flask server entry point
│   ├── templates/
│   │   ├── base.html
│   │   ├── dashboard.html
│   │   └── designer.html
│   └── static/style.css
├── compiler/
│   └── compile_sequence.py ← JSON → .bin compiler
├── event_codes.py          ← shared event code definitions
├── employees.json          ← badge ID → name/role map
├── .env                    ← FLASK_HOST, FLASK_PORT, FLASK_DEBUG
├── requirements.txt
├── plc_registry/
│   └── devpi01.json        ← live state for the bench PLC
├── sequences/
│   ├── definitions/        ← editable JSON sequence definitions
│   └── compiled/           ← compiled .bin files served to PLCs
└── logs/
    └── devpi01/
        ├── YYYY-MM-DD.jsonl
        └── production.csv
```

---

## Updating the Pi After Code Changes

```bash
# From this machine — sync the pi/ directory to the Pi
rsync -av --delete \
  /home/nicholasjarosz/STM32CubeIDE/workspace_1.14.0/PLC_MIGRATION/pi/ \
  pi@10.0.0.67:~/plc_server/

# Then restart the service
ssh pi@10.0.0.67 "sudo systemctl restart plc-server"
```
