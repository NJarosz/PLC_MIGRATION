#include "sequence_engine.h"
#include "main.h"
#include "state_machine.h"      // For System_t and system_state
#include "outputs.h"            // For Outputs_t and outputs
#include "logger.h"
#include "supervisor_comms.h"   // For SupervisorComms_RequestUpload


static SequenceStep_t sequence_table[MAX_SEQUENCE_STEPS];
static uint8_t sequence_length = 0;

extern System_t system_state;
extern Outputs_t outputs;

void SequenceEngine_Init(void)
{
    sequence_length = 0;
}

SequenceStep_t* SequenceEngine_GetTable(void)
{
    return sequence_table;
}

uint8_t SequenceEngine_GetLength(void)
{
    return sequence_length;
}

void SequenceEngine_SetLength(uint8_t len)
{
    sequence_length = len;
}

void SequenceEngine_Update(void)
{
    if (system_state.state != STATE_RUNNING)
    {
        for (int i = 0; i < NUM_RELAYS; i++)
            outputs.relay_requested[i] = false;
        return;
    }

    if (!system_state.sequence_active)
        return;

    uint32_t now = HAL_GetTick();

    if (system_state.current_step >= sequence_length) {
        system_state.sequence_active = false;
        for (int i = 0; i < NUM_RELAYS; i++) {
            outputs.relay_requested[i] = false;
        }
        Logger_Log(LOG_TIER_A3, EVENT_SEQUENCE_COMPLETE, sequence_length);
        SupervisorComms_RequestUpload();
        return;
    }

    SequenceStep_t *step = &sequence_table[system_state.current_step];

    for (int i = 0; i < NUM_RELAYS; i++)
    {
        outputs.relay_requested[i] =
            (step->relay_mask & (1 << i)) != 0;
    }

    if ((now - system_state.step_start_time) >= step->duration_ms)
    {
        system_state.current_step++;
        system_state.step_start_time = now;
    }
}
