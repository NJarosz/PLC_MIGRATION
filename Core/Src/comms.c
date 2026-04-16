#include "comms.h"
#include "sequence_storage.h"
#include "sequence_engine.h"
#include "main.h"
#include <string.h>

#define START_BYTE 0xAA
#define END_BYTE   0x55

static uint8_t rx_buffer[256];

extern UART_HandleTypeDef huart2;

void Comms_Init(void) {

}

void Comms_Task(void) {
    uint8_t byte;

    if (HAL_UART_Receive(&huart2, &byte, 1, 0) == HAL_OK) {
        if (byte == START_BYTE) {
            uint8_t length;

            HAL_UART_Receive(&huart2, &length, 1, 0);

            HAL_UART_Receive(&huart2, rx_buffer, length, 0);

            uint8_t end;
            HAL_UART_Receive(&huart2, &end, 1, 0);

            if (end != END_BYTE)
                return;

            SequenceStep_t steps[32];
            uint8_t step_count = length / sizeof(SequenceStep_t);

            memcpy(steps, rx_buffer, length);

            SequenceStorage_Save(steps, step_count);

            printf("NEW SEQUENCE LOADED (%d steps)\r\n", step_count);
        }
    }
}
