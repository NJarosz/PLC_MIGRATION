"""
event_codes.py — STM32 event code definitions for Pi-side log processing.

Keep in sync with EventCode_t in Core/Inc/logger.h.
"""

# Maps event code integer → short name string
EVENT_NAMES = {
    100: "SAFETY_ESTOP",
    101: "SAFETY_RESET",
    200: "STATE_BOOT",
    201: "BOOT_COMPLETE",
    300: "STATE_IDLE",
    301: "SEQUENCE_REQUESTED",
    302: "SEQUENCE_RECEIVED",
    310: "COMMS_RX_TIMEOUT",
    311: "COMMS_FRAME_INVALID",
    312: "COMMS_UART_ERROR",
    313: "COMMS_HASH_MISMATCH",
    314: "COMMS_META_INVALID",
    400: "WATCHDOG_RESET",
    401: "A1_OVERFLOW",
    600: "LOGIN",
    601: "SEQUENCE_START",
    602: "LOGOUT",
    603: "SEQUENCE_COMPLETE",
    700: "DEBUG_STEP_TIMING",
}

# Maps tier integer → tier name string (matches LogTier_t enum)
TIER_NAMES = {0: "A1", 1: "A2", 2: "A3", 3: "B", 4: "C"}

# Event codes that generate a row in the production CSV.
# Key: event code.  Value: EventType string written to CSV.
PRODUCTION_EVENT_TYPES = {
    600: "LOGIN",
    601: "SEQUENCE_START",
    602: "LOGOUT",
    603: "SEQUENCE_END",
    100: "FAULT_ESTOP",
    101: "FAULT_RESET",
    400: "FAULT_WATCHDOG",
    401: "FAULT_A1_OVERFLOW",
}

# Event codes that indicate the STM32 has rebooted — invalidates tick calibration.
REBOOT_EVENTS = {200, 400}  # STATE_BOOT, WATCHDOG_RESET
