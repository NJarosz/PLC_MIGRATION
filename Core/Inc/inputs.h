#ifndef INPUTS_H
#define INPUTS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    // E-stop (dedicated hardware switch)
    bool     estop;               // true = E-stop is currently engaged
    bool     estop_last;
    bool     estop_release_edge;  // one cycle: E-stop just released → auto-reset

    // PB0 — Run: starts sequence from ARMED; pairs with ACK for bypass hold
    bool     run;
    bool     run_last;
    bool     run_rising_edge;

    // PB1 — Fetch/Logoff: sequence fetch from IDLE; logoff from ARMED
    bool     bypass;
    bool     bypass_last;
    bool     bypass_rising_edge;

    // ACK button (PB10) — count reset (two-step); goal ack in GOAL_MET
    bool     ack;
    bool     ack_last;
    bool     ack_rising_edge;

    // Combo: PB0 + ACK held simultaneously for 1 s → bypass arm from IDLE
    bool     both_held_1s;        // fires once per hold

    // RFID
    bool     rfid_rising_edge;    // true for one scan cycle on each new badge tap
    uint32_t rfid_employee_id;    // employee number from last successful card read
} Inputs_t;

void Inputs_Init(void);
void Inputs_Update(void);
void Inputs_EnableRFID(bool enabled);  // IDLE-only polling gate

extern Inputs_t inputs;

#endif
