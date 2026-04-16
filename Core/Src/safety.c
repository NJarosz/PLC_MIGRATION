#include "safety.h"
#include "inputs.h"
#include "logger.h"

bool ESTOP_ASSERTED = false;
bool FAULT_LATCHED = false;

void Safety_Init(void) {
    ESTOP_ASSERTED = false;
    FAULT_LATCHED = false;
}

bool Safety_IsOK(void) {
    // Check for ESTOP
    if (inputs.estop) {
        ESTOP_ASSERTED = true;
        Logger_Log(LOG_TIER_A1, EVENT_SAFETY_ESTOP, 0);
        return false;
    }

    // Add other safety checks here (e.g., door sensors, etc.)

    return !ESTOP_ASSERTED && !FAULT_LATCHED;
}
