#include "safety.h"
#include "inputs.h"
#include "logger.h"
#include "supervisor_comms.h"

bool ESTOP_ASSERTED = false;
bool FAULT_LATCHED = false;

void Safety_Init(void) {
    ESTOP_ASSERTED = false;
    FAULT_LATCHED = false;
}

bool Safety_IsOK(void) {
    if (inputs.estop) {
        // Log and upload only on the rising edge — not every scan while held
        if (!ESTOP_ASSERTED) {
            ESTOP_ASSERTED = true;
            Logger_Log(LOG_TIER_A1, EVENT_SAFETY_ESTOP, 0);
            SupervisorComms_RequestUpload();
        }
        return false;
    }

    return !ESTOP_ASSERTED && !FAULT_LATCHED;
}
