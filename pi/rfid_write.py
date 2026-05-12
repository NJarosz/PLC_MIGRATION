#!/usr/bin/env python3
"""
Write an employee ID to an RC522 MIFARE Classic card.
The ID is stored as an ASCII decimal string at block 8 (e.g. "1003" = 31 30 30 33),
matching how existing shop floor cards were written and how the STM32 parses them.

Usage:
    python3 rfid_write.py
"""

import time
import RPi.GPIO as GPIO
from mfrc522 import MFRC522

EMPLOYEE_BLOCK = 8                          # must match RFID_EMPLOYEE_BLOCK in mfrc522.h
KEY_A = [0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF]

reader = MFRC522()

def wait_for_card():
    while True:
        status, _ = reader.MFRC522_Request(reader.PICC_REQIDL)
        if status == reader.MI_OK:
            status, uid = reader.MFRC522_Anticoll()
            if status == reader.MI_OK:
                return uid
        time.sleep(0.1)

def write_employee_id(uid, emp_id):
    reader.MFRC522_SelectTag(uid)

    status = reader.MFRC522_Auth(reader.PICC_AUTHENT1A, EMPLOYEE_BLOCK, KEY_A, uid)
    if status != reader.MI_OK:
        print("  Auth failed — cannot write")
        reader.MFRC522_StopCrypto1()
        return False

    # Store as ASCII decimal string, pad remainder with spaces to match existing card format
    id_bytes = str(emp_id).encode('ascii')
    data = list(id_bytes) + [0x20] * (16 - len(id_bytes))

    status = reader.MFRC522_Write(EMPLOYEE_BLOCK, data)
    reader.MFRC522_StopCrypto1()

    if status != reader.MI_OK:
        print("  Write failed")
        return False

    return True

def verify_card(uid, expected_id):
    reader.MFRC522_SelectTag(uid)

    status = reader.MFRC522_Auth(reader.PICC_AUTHENT1A, EMPLOYEE_BLOCK, KEY_A, uid)
    if status != reader.MI_OK:
        reader.MFRC522_StopCrypto1()
        return None

    data = reader.MFRC522_Read(EMPLOYEE_BLOCK)
    reader.MFRC522_StopCrypto1()

    if not data:
        return None

    return int(bytes(data[:4]).decode('ascii').strip())

try:
    while True:
        emp_id_str = input("Enter employee ID to write (or 'q' to quit): ").strip()
        if emp_id_str.lower() == 'q':
            break

        try:
            emp_id = int(emp_id_str)
            if emp_id <= 0 or emp_id > 0xFFFFFFFF:
                print("ID must be between 1 and 4294967295")
                continue
        except ValueError:
            print("Invalid number")
            continue

        print(f"Hold card to reader to write ID {emp_id}...")
        uid = wait_for_card()
        uid_str = ' '.join(f'{b:02X}' for b in uid[:4])
        print(f"  Card UID: {uid_str}")

        if write_employee_id(uid, emp_id):
            # Re-select for verify
            time.sleep(0.1)
            uid = wait_for_card()
            verified = verify_card(uid, emp_id)
            if verified == emp_id:
                print(f"  Written and verified: employee ID = {emp_id}")
            else:
                print(f"  Write succeeded but verify read back {verified} — check card")

        print()
        time.sleep(1)

except KeyboardInterrupt:
    pass
finally:
    GPIO.cleanup()
    print("Done")
