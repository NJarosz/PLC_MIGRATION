#ifndef INPUTS_H
#define INPUTS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool estop;
    bool run;
    bool run_last;
    bool run_rising_edge;
    bool rfid;
    bool rfid_last;
    bool rfid_rising_edge;
} Inputs_t;

void Inputs_Init(void);
void Inputs_Update(void);

extern Inputs_t inputs;

#endif
