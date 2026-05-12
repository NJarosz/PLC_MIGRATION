#!/usr/bin/env python3
"""
RC522 reader test — scans all data blocks on a MIFARE Classic 1K card,
shows raw bytes and uint32 value, and highlights the target employee ID
and the configured employee block.

Requirements:
    pip install mfrc522

SPI must be enabled on the Pi:
    sudo raspi-config → Interface Options → SPI → Enable
"""

import struct
import time
import RPi.GPIO as GPIO
from mfrc522 import MFRC522

EMPLOYEE_BLOCK = 8                              # must match RFID_EMPLOYEE_BLOCK in mfrc522.h
KEY_A = [0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF]
TARGET_ID = 1003                                # change to whatever ID you're looking for

SECTORS = 16
BLOCKS_PER_SECTOR = 4

reader = MFRC522()

def scan_card(uid):
    found_blocks = []

    for sector in range(SECTORS):
        trailer = sector * BLOCKS_PER_SECTOR + 3

        status = reader.MFRC522_Auth(reader.PICC_AUTHENT1A, trailer, KEY_A, uid)
        if status != reader.MI_OK:
            print(f"  Sector {sector:2d}: auth failed — skipping")
            continue

        for block in range(sector * BLOCKS_PER_SECTOR, trailer):
            data = reader.MFRC522_Read(block)
            if not data:
                print(f"    Block {block:2d}: read failed")
                continue

            raw = bytes(data[:16])
            val = struct.unpack_from('<I', raw)[0]

            markers = []
            if val == TARGET_ID:
                markers.append(f"<-- ID {TARGET_ID} HERE")
                found_blocks.append(block)
            if block == EMPLOYEE_BLOCK:
                markers.append("<-- MCU reads this block")

            marker_str = "  " + "  ".join(markers) if markers else ""
            print(f"    Block {block:2d}: {raw[:4].hex(' ')}  (uint32={val}){marker_str}")

    reader.MFRC522_StopCrypto1()
    return found_blocks

print(f"RC522 scan — looking for ID {TARGET_ID} across all blocks (MCU reads block {EMPLOYEE_BLOCK})")
print("Hold a card to the reader. Ctrl+C to exit.\n")

try:
    while True:
        status, _ = reader.MFRC522_Request(reader.PICC_REQIDL)
        if status != reader.MI_OK:
            time.sleep(0.1)
            continue

        status, uid = reader.MFRC522_Anticoll()
        if status != reader.MI_OK:
            print("Card detected but anti-collision failed")
            continue

        uid_str = ' '.join(f'{b:02X}' for b in uid[:4])
        print(f"Card UID: {uid_str}")
        reader.MFRC522_SelectTag(uid)

        found = scan_card(uid)

        if found:
            print(f"\n  ID {TARGET_ID} found at block(s): {found}")
            if EMPLOYEE_BLOCK not in found:
                print(f"  *** MCU is reading block {EMPLOYEE_BLOCK} — update RFID_EMPLOYEE_BLOCK in mfrc522.h to match ***")
        else:
            print(f"\n  ID {TARGET_ID} not found on this card — card may not be written yet")

        print()
        time.sleep(2)

except KeyboardInterrupt:
    print("Done")
finally:
    GPIO.cleanup()
