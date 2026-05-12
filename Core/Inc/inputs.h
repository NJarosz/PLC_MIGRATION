#ifndef INPUTS_H
#define INPUTS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool     estop;
    bool     run;
    bool     run_last;
    bool     run_rising_edge;
    bool     bypass;                 // PB1: current debounced state
    bool     bypass_last;
    bool     bypass_rising_edge;     // one cycle: first press detected
    bool     bypass_short_release;   // one cycle: released in < 2s  → logoff / continue
    bool     bypass_hold_2s;         // one cycle: held ≥ 2s         → count reset
    bool     rfid_rising_edge;       // true for one scan cycle on each new badge tap
    uint32_t rfid_employee_id;       // employee number from last successful card read
} Inputs_t;

void Inputs_Init(void);
void Inputs_Update(void);
void Inputs_EnableRFID(bool enabled);  // called by state machine; polling only active in IDLE

extern Inputs_t inputs;

#endif
