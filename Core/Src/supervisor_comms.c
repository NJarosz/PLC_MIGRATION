#include "supervisor_comms.h"
#include "main.h"               // for huart1
#include "sequence_storage.h"
#include "sequence_engine.h"
#include "logger.h"
#include "sha256.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>

#define START_BYTE 0xAA
#define END_BYTE   0x55
#define HEARTBEAT_INTERVAL_MS 5000  // 5s

typedef enum {
    RX_WAIT_START,
    RX_WAIT_LEN,
    RX_WAIT_META,    // collect META_SIZE bytes of metadata after LEN
    RX_WAIT_PAYLOAD,
    RX_WAIT_HASH,    // collect SHA256_BLOCK_SIZE bytes after payload
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
                rx_state = RX_WAIT_META;
            }

            if (rx_state == RX_WAIT_META &&
                capture_idx >= (2 + META_SIZE)) {
                rx_state = RX_WAIT_PAYLOAD;
            }

            if (rx_state == RX_WAIT_PAYLOAD &&
                capture_idx >= (2 + META_SIZE + rx_len)) {
                rx_state = RX_WAIT_HASH;
            }

            if (rx_state == RX_WAIT_HASH &&
                capture_idx >= (2 + META_SIZE + rx_len + SHA256_BLOCK_SIZE)) {
                rx_state = RX_WAIT_END;
            }

            if (rx_state == RX_WAIT_END && byte == END_BYTE) {
                // Verify SHA-256: hash covers meta + blob
                uint8_t computed_hash[SHA256_BLOCK_SIZE];
                SHA256_CTX sha_ctx;
                sha256_init(&sha_ctx);
                sha256_update(&sha_ctx, &capture_buffer[2], META_SIZE + rx_len);
                sha256_final(&sha_ctx, computed_hash);

                const uint8_t *received_hash = &capture_buffer[2 + META_SIZE + rx_len];

                if (memcmp(computed_hash, received_hash, SHA256_BLOCK_SIZE) != 0) {
                    Logger_Log(LOG_TIER_B, EVENT_COMMS_HASH_MISMATCH, 0);
                    capturing = false;
                    rx_state = RX_WAIT_START;
                } else if (rx_len > 0 && rx_len % sizeof(SequenceStep_t) == 0) {
                    // Parse and validate metadata
                    SequenceMetadata_t meta;
                    memcpy(&meta, &capture_buffer[2], META_SIZE);
                    uint8_t expected_steps = rx_len / sizeof(SequenceStep_t);

                    if (meta.step_count != expected_steps) {
                        Logger_Log(LOG_TIER_B, EVENT_COMMS_META_INVALID, meta.step_count);
                        capturing = false;
                        rx_state = RX_WAIT_START;
                    } else {
                        SequenceStep_t steps[MAX_SEQUENCE_STEPS];
                        memcpy(steps, &capture_buffer[2 + META_SIZE], rx_len);
                        SequenceStorage_Save(steps, expected_steps);
                        Logger_Log(LOG_TIER_B, EVENT_SEQUENCE_RECEIVED, expected_steps);

                        // LCD mock — replace with actual LCD driver calls when hardware is ready
                        printf("--- SEQUENCE LOADED ---\r\n");
                        printf("Seq : %.*s\r\n",  15, (char*)meta.seq_name);
                        printf("Part: %.*s\r\n",  11, (char*)meta.part_num);
                        printf("Mach: %.*s\r\n",   7, (char*)meta.machine_id);
                        printf("Steps: %u  v%u\r\n", expected_steps, meta.version);
                        printf("-----------------------\r\n");

                        capturing = false;
                        rx_state = RX_WAIT_START;
                    }
                } else {
                    Logger_Log(LOG_TIER_B, EVENT_COMMS_FRAME_INVALID, rx_len);
                    capturing = false;
                    rx_state = RX_WAIT_START;
                }
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
