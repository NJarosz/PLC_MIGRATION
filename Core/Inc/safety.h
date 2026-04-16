#ifndef SAFETY_H
#define SAFETY_H

#include <stdint.h>
#include <stdbool.h>

extern bool ESTOP_ASSERTED;
extern bool FAULT_LATCHED;

void Safety_Init(void);
bool Safety_IsOK(void);

#endif
