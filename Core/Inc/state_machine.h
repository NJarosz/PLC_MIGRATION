#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    STATE_BOOT = 0,
    STATE_IDLE,
    STATE_ARMED,
    STATE_RUNNING,
    STATE_FAULT
} SystemState_t;

typedef struct {
    SystemState_t state;
    uint32_t state_entry_time;
    bool sequence_active;
    uint8_t current_step;
    uint32_t step_start_time;
    bool fault_request;
} System_t;

void StateMachine_Init(void);
void StateMachine_Update(bool safety_ok);

extern System_t system_state;

#endif
