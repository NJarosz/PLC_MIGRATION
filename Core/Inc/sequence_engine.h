#ifndef SEQUENCE_ENGINE_H
#define SEQUENCE_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"   // for NUM_RELAYS

#define MAX_SEQUENCE_STEPS 32

typedef struct {
    uint8_t relay_mask;
    uint32_t duration_ms;
} __attribute__((packed)) SequenceStep_t;   // <-- Add packed

void SequenceEngine_Init(void);
void SequenceEngine_Update(void);

SequenceStep_t* SequenceEngine_GetTable(void);
uint8_t SequenceEngine_GetLength(void);
void SequenceEngine_SetLength(uint8_t len);

#endif
