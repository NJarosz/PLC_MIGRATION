#ifndef LOGGER_H
#define LOGGER_H

#define ENABLE_DEBUG_LOGS 1

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    LOG_TIER_A1 = 0,
    LOG_TIER_A2,
    LOG_TIER_A3,
    LOG_TIER_B,
    LOG_TIER_C
} LogTier_t;

typedef struct {
    uint32_t timestamp_ms;
    LogTier_t tier;
    uint16_t event_code;
    uint32_t data;
} LogEvent_t;

// Event codes (example definitions; expand as needed)
typedef enum {
    EVENT_SAFETY_ESTOP = 100,
	EVENT_SAFETY_RESET = 101,
    EVENT_STATE_BOOT = 200,
    EVENT_BOOT_COMPLETE = 201,
    EVENT_STATE_IDLE = 300,
    EVENT_SEQUENCE_REQUESTED  = 301,  // STM32 sent REQUEST_SEQUENCE to ESP32
    EVENT_SEQUENCE_RECEIVED   = 302,  // valid frame stored; data = step count
    EVENT_COMMS_RX_TIMEOUT    = 310,  // partial frame abandoned after 800ms; data = bytes captured
    EVENT_COMMS_FRAME_INVALID = 311,  // frame length not a multiple of step size; data = rx_len
    EVENT_COMMS_UART_ERROR    = 312,  // HAL_UART_Receive returned HAL_ERROR; data = UART SR
    EVENT_COMMS_HASH_MISMATCH = 313,  // received hash did not match computed hash; frame rejected
    EVENT_COMMS_META_INVALID  = 314,  // metadata step_count disagrees with blob length; data = step_count from meta
    EVENT_LOGIN = 600,
	EVENT_SEQUENCE_START = 601,
    EVENT_LOGOUT = 602,
    EVENT_DEBUG_STEP_TIMING = 700

} EventCode_t;

#define LOG_A1_BUFFER_SIZE 16
#define LOG_GENERAL_BUFFER_SIZE 64

void Logger_Init(void);
void Logger_Log(LogTier_t tier, uint16_t event_code, uint32_t data);
bool Logger_A1_Overflowed(void);
void Logger_Process(void);

#endif
