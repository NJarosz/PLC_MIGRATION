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

import hashlib
import struct
import json
import sys
import os
import argparse
from datetime import datetime, timezone

# Must match STM32 SequenceStep_t: __attribute__((packed)) { uint8_t relay_mask; uint32_t duration_ms; }
STEP_FORMAT = "<BI"
STEP_SIZE   = struct.calcsize(STEP_FORMAT)   # 5 bytes
MAX_STEPS   = 32

# Maps human relay numbers (1–4) to their bitmask values.
# Derived from sequence_engine.c: relay_requested[i] = (relay_mask & (1 << i)) != 0
# Relay 1 = bit 0 = 0x01, Relay 2 = bit 1 = 0x02, Relay 3 = bit 2 = 0x04, Relay 4 = bit 3 = 0x08
RELAY_BITS = {1: 0x01, 2: 0x02, 3: 0x04, 4: 0x08}
NUM_RELAYS = len(RELAY_BITS)


def relays_to_mask(relays: list, step_index: int) -> int:
    """Convert a list of relay numbers (e.g. [1, 3]) to a bitmask (e.g. 0x05)."""
    if not isinstance(relays, list):
        raise ValueError(f"Step {step_index}: 'relays' must be a list, got {relays!r}")
    seen = set()
    mask = 0
    for r in relays:
        if not isinstance(r, int) or r not in RELAY_BITS:
            raise ValueError(
                f"Step {step_index}: relay {r!r} is invalid — must be an integer in 1–{NUM_RELAYS}"
            )
        if r in seen:
            raise ValueError(f"Step {step_index}: relay {r} is listed more than once")
        seen.add(r)
        mask |= RELAY_BITS[r]
    return mask


def validate(definition: dict):
    for field in ("sequence_id", "version", "target_plc", "steps"):
        if field not in definition:
            raise ValueError(f"Missing required field: '{field}'")

    steps = definition["steps"]
    if not isinstance(steps, list) or not (1 <= len(steps) <= MAX_STEPS):
        raise ValueError(f"'steps' must be a list of 1–{MAX_STEPS} entries, got {len(steps)}")

    for i, step in enumerate(steps):
        if "relays" not in step:
            raise ValueError(f"Step {i}: missing 'relays' field")
        if "duration_ms" not in step:
            raise ValueError(f"Step {i}: missing 'duration_ms' field")
        relays_to_mask(step["relays"], i)   # validates relay numbers and duplicates
        duration = step["duration_ms"]
        if not isinstance(duration, int) or duration < 0:
            raise ValueError(f"Step {i}: duration_ms must be a non-negative integer, got {duration!r}")


def compile_sequence(def_path: str, output_dir: str = "sequences/compiled") -> tuple[str, dict]:
    with open(def_path) as f:
        definition = json.load(f)

    validate(definition)

    seq_id  = definition["sequence_id"]
    version = definition["version"]
    steps   = definition["steps"]

    os.makedirs(output_dir, exist_ok=True)

    blob = b"".join(
        struct.pack(STEP_FORMAT, relays_to_mask(step["relays"], i), step["duration_ms"])
        for i, step in enumerate(steps)
    )

    # Append SHA-256 of the sequence payload — verified by STM32 before storing
    digest = hashlib.sha256(blob).digest()   # 32 bytes
    signed = blob + digest

    out_filename = f"{seq_id}_v{version}.bin"
    out_path     = os.path.join(output_dir, out_filename)
    with open(out_path, "wb") as f:
        f.write(signed)

    print(f"[compile] {out_path}  ({len(blob)} bytes payload + 32 bytes SHA-256)")
    print(f"[compile] hash: {digest.hex()}")
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
