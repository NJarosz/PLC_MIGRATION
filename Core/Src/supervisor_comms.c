#include "supervisor_comms.h"
#include "main.h"               // for huart1
#include "sequence_storage.h"
#include "sequence_engine.h"
#include "logger.h"
#include "usart.h"
#include <string.h>

#define START_BYTE 0xAA
#define END_BYTE   0x55
#define HEARTBEAT_INTERVAL_MS 5000  // 5s

typedef enum {
    RX_WAIT_START,
    RX_WAIT_LEN,
    RX_WAIT_PAYLOAD,
    RX_WAIT_END
} RxState_t;

static RxState_t rx_state = RX_WAIT_START;
static uint8_t rx_len = 0;
static uint8_t rx_idx = 0;
static uint8_t rx_buffer[256];

static uint32_t last_heartbeat = 0;
static bool connected = false;  // Track Pi connectivity

void SupervisorComms_Init(void) {
    // Optional: ESP reset pin if available
    last_heartbeat = HAL_GetTick();
    connected = false;
}

bool SupervisorComms_IsConnected(void) {
    return connected;
}


// Call this when operator wants a new sequence (e.g. button press in IDLE)
void RequestNewSequence(void)
{
    const char *request = "REQUEST_SEQUENCE\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t*)request, strlen(request), 100);
    Logger_Log(LOG_TIER_B, EVENT_SEQUENCE_REQUESTED, 0);
}


void SupervisorComms_Task(void)
{
    static bool first_run = true;
    if (first_run) {
        uint8_t tmp;
        while (HAL_UART_Receive(&huart1, &tmp, 1, 10) == HAL_OK);  // flush junk
        first_run = false;
    }

    uint8_t byte;
    static uint32_t last_rx_time = 0;
    static uint8_t capture_buffer[256];
    static uint8_t capture_idx = 0;
    static bool capturing = false;

    HAL_StatusTypeDef rx_status;

    while ((rx_status = HAL_UART_Receive(&huart1, &byte, 1, 0)) == HAL_OK)
    {
        last_rx_time = HAL_GetTick();

        if (byte == START_BYTE && !capturing) {
            capture_idx = 0;
            capturing = true;
            capture_buffer[capture_idx++] = byte;
            rx_state = RX_WAIT_LEN;
            continue;
        }

        if (capturing) {
            capture_buffer[capture_idx++] = byte;

            if (capture_idx >= 2 && rx_state == RX_WAIT_LEN) {
                rx_len = capture_buffer[1];
                rx_state = RX_WAIT_PAYLOAD;
            }

            if (rx_state == RX_WAIT_PAYLOAD && capture_idx >= (2 + rx_len)) {
                rx_state = RX_WAIT_END;
            }

            if (rx_state == RX_WAIT_END && byte == END_BYTE) {
                if (rx_len > 0 && rx_len % sizeof(SequenceStep_t) == 0) {
                    uint8_t step_count = rx_len / sizeof(SequenceStep_t);
                    SequenceStep_t steps[MAX_SEQUENCE_STEPS];
                    memcpy(steps, &capture_buffer[2], rx_len);   // skip START + LEN
                    SequenceStorage_Save(steps, step_count);
                    Logger_Log(LOG_TIER_B, EVENT_SEQUENCE_RECEIVED, step_count);
                } else {
                    Logger_Log(LOG_TIER_B, EVENT_COMMS_FRAME_INVALID, rx_len);
                }

                capturing = false;
                rx_state = RX_WAIT_START;
            }
        }
    }

    if (rx_status == HAL_ERROR) {
        Logger_Log(LOG_TIER_B, EVENT_COMMS_UART_ERROR, huart1.Instance->SR);
    }

    // Timeout safety
    if (capturing && (HAL_GetTick() - last_rx_time > 800)) {
        Logger_Log(LOG_TIER_B, EVENT_COMMS_RX_TIMEOUT, capture_idx);
        capturing = false;
        rx_state = RX_WAIT_START;
    }

//    // Heartbeat TX (non-blocking)
//    uint32_t now = HAL_GetTick();
//    if (now - last_heartbeat >= HEARTBEAT_INTERVAL_MS) {
//        uint8_t hb[4] = {0xBB, 0x01, 0x00, 0xEE};  // Simple placeholder frame; expand with tick/status
//        HAL_UART_Transmit(&huart1, hb, sizeof(hb), 10);  // Short timeout
//        last_heartbeat = now;
//        // Future: If no ACK in N cycles, set connected = false; log Tier B
//    }

    // Future: Process incoming ACKs/logs in RX state machine
}
