#ifndef OUTPUTS_H
#define OUTPUTS_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"  // For NUM_RELAYS, relay pins/ports

typedef struct {
    bool relay_requested[NUM_RELAYS];
    bool relay_actual[NUM_RELAYS];
} Outputs_t;

void Outputs_Init(void);
void Outputs_Apply(bool safety_ok);

extern Outputs_t outputs;

#endif
