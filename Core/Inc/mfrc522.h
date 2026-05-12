#ifndef MFRC522_H
#define MFRC522_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

// Sector/block where employee ID is stored on the MIFARE Classic card.
// Block 8 = first data block of sector 2.
// First 4 bytes are the employee ID as a little-endian uint32.
#define RFID_EMPLOYEE_BLOCK   8

typedef enum {
    MFRC522_OK       = 0,
    MFRC522_NOTAG    = 1,
    MFRC522_ERR      = 2,
} MFRC522_Status_t;

typedef struct {
    uint8_t size;
    uint8_t uid[10];
    uint8_t sak;
} MFRC522_UID_t;

void             MFRC522_Init(void);
uint32_t         MFRC522_ReadEmployeeID(void);  // 0 = no card / error

#endif
