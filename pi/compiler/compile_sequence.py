#!/usr/bin/env python3
"""
compile_sequence.py

Compiles a JSON sequence definition into a binary blob compatible with
the STM32 SequenceStep_t struct (little-endian: uint8 relay_mask + uint32 duration_ms).

Usage:
    python3 compile_sequence.py <definition.json>           # compile only
    python3 compile_sequence.py <definition.json> --deploy  # compile + update PLC registry

Output:
    sequences/compiled/<sequence_id>_v<version>.bin
"""

import struct
import json
import sys
import os
import argparse
from datetime import datetime, timezone

# Must match STM32 SequenceStep_t: __attribute__((packed)) { uint8_t relay_mask; uint32_t duration_ms; }
STEP_FORMAT   = "<BI"
STEP_SIZE     = struct.calcsize(STEP_FORMAT)   # 5 bytes
MAX_STEPS     = 32
MAX_RELAY_MASK = 0x0F   # 4 relays (bits 0-3)


def validate(definition: dict):
    for field in ("sequence_id", "version", "target_plc", "steps"):
        if field not in definition:
            raise ValueError(f"Missing required field: '{field}'")

    steps = definition["steps"]
    if not isinstance(steps, list) or not (1 <= len(steps) <= MAX_STEPS):
        raise ValueError(f"steps must be a list of 1–{MAX_STEPS} entries, got {len(steps)}")

    for i, step in enumerate(steps):
        mask     = step.get("relay_mask")
        duration = step.get("duration_ms")
        if mask is None or duration is None:
            raise ValueError(f"Step {i}: missing 'relay_mask' or 'duration_ms'")
        if not isinstance(mask, int) or not (0 <= mask <= MAX_RELAY_MASK):
            raise ValueError(f"Step {i}: relay_mask {mask!r} must be int in [0, {MAX_RELAY_MASK}]")
        if not isinstance(duration, int) or duration <= 0:
            raise ValueError(f"Step {i}: duration_ms {duration!r} must be a positive integer")


def compile_sequence(def_path: str, output_dir: str = "sequences/compiled") -> tuple[str, dict]:
    with open(def_path) as f:
        definition = json.load(f)

    validate(definition)

    seq_id  = definition["sequence_id"]
    version = definition["version"]
    steps   = definition["steps"]

    os.makedirs(output_dir, exist_ok=True)

    blob = b"".join(
        struct.pack(STEP_FORMAT, step["relay_mask"], step["duration_ms"])
        for step in steps
    )

    out_filename = f"{seq_id}_v{version}.bin"
    out_path     = os.path.join(output_dir, out_filename)
    with open(out_path, "wb") as f:
        f.write(blob)

    print(f"[compile] {out_path}  ({len(blob)} bytes, {len(steps)} steps)")
    return out_path, definition


def deploy(definition: dict, bin_path: str, registry_dir: str = "plc_registry"):
    plc_id        = definition["target_plc"]
    registry_path = os.path.join(registry_dir, f"{plc_id}.json")

    os.makedirs(registry_dir, exist_ok=True)

    if os.path.exists(registry_path):
        with open(registry_path) as f:
            registry = json.load(f)
    else:
        registry = {"plc_id": plc_id, "description": ""}

    registry["active_sequence"] = definition["sequence_id"]
    registry["active_version"]  = definition["version"]
    registry["active_bin"]      = bin_path   # relative path from server root
    registry["deployed_at"]     = datetime.now(timezone.utc).isoformat()

    with open(registry_path, "w") as f:
        json.dump(registry, f, indent=2)

    print(f"[deploy]  {plc_id} → {definition['sequence_id']} v{definition['version']}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Compile a JSON sequence definition to a .bin blob")
    parser.add_argument("definition",     help="Path to sequence definition JSON")
    parser.add_argument("--deploy",       action="store_true",
                        help="Update the target PLC registry entry after compiling")
    parser.add_argument("--output-dir",   default="sequences/compiled",
                        help="Directory for compiled .bin files (default: sequences/compiled)")
    parser.add_argument("--registry-dir", default="plc_registry",
                        help="PLC registry directory (default: plc_registry)")
    args = parser.parse_args()

    try:
        bin_path, definition = compile_sequence(args.definition, args.output_dir)
        if args.deploy:
            deploy(definition, bin_path, args.registry_dir)
    except (ValueError, KeyError, FileNotFoundError) as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
