#include "state_machine.h"
#include "main.h"
#include "inputs.h"
#include "logger.h"
#include "sequence_engine.h"
#include "safety.h"
#include "supervisor_comms.h"
#include "lcd.h"
#include <stdio.h>

#define ENTER_IDLE() do { \
    system_state.state = STATE_IDLE; \
    system_state.state_entry_time = now; \
    system_state.count_reset_pending = false; \
    Inputs_EnableRFID(true); \
    LCD_ShowIdle(SupervisorComms_GetActiveSeqName(), \
                 SupervisorComms_GetCount(), \
                 SupervisorComms_GetPartNum()); \
} while(0)

#define LEAVE_IDLE() do { \
    Inputs_EnableRFID(false); \
    SupervisorComms_ClearNewSequence(); \
} while(0)


System_t system_state;

void StateMachine_Init(void) {
    system_state.state = STATE_BOOT;
    system_state.state_entry_time = HAL_GetTick();
    system_state.sequence_active = false;
    system_state.current_step = 0;
    system_state.step_start_time = 0;
    system_state.fault_request = false;
    system_state.count_reset_pending = false;
}

// Two-step count reset via ACK button.
// Returns true if ACK was consumed (caller should skip its normal action this cycle).
static bool handle_count_reset(uint32_t now, SystemState_t restore_state) {
    (void)now;
    if (!inputs.ack_rising_edge) return false;

    if (!system_state.count_reset_pending) {
        system_state.count_reset_pending = true;
        LCD_ShowCountResetConfirm(SupervisorComms_GetCount());
        return true;
    } else {
        uint16_t prev = SupervisorComms_GetCount();
        SupervisorComms_ResetCount();
        Logger_Log(LOG_TIER_B, EVENT_COUNT_RESET, prev);
        system_state.count_reset_pending = false;
        if (restore_state == STATE_IDLE) {
            LCD_ShowIdle(SupervisorComms_GetActiveSeqName(),
                         SupervisorComms_GetCount(),
                         SupervisorComms_GetPartNum());
        } else {
            LCD_ShowArmed(SupervisorComms_GetOperatorName(),
                          SupervisorComms_GetCount(), SupervisorComms_GetGoal(),
                          SupervisorComms_GetPartNum());
        }
        return true;
    }
}

static void cancel_count_reset(void) {
    system_state.count_reset_pending = false;
}

void StateMachine_Update(bool safety_ok) {
    uint32_t now = HAL_GetTick();

    if (Logger_A1_Overflowed() && system_state.state != STATE_FAULT) {
        Logger_Log(LOG_TIER_B, EVENT_A1_OVERFLOW, 0);
        system_state.fault_request = true;
    }

    if (system_state.fault_request) {
        system_state.state = STATE_FAULT;
        system_state.state_entry_time = now;
        system_state.sequence_active = false;
        system_state.count_reset_pending = false;
        system_state.fault_request = false;
        LCD_ShowFault();
    }

    switch (system_state.state) {
        case STATE_BOOT:
            Logger_Log(LOG_TIER_B, EVENT_STATE_BOOT, system_state.state);
            ENTER_IDLE();
            Logger_Log(LOG_TIER_B, EVENT_BOOT_COMPLETE, now);
            break;

        case STATE_IDLE: {
            if (!safety_ok) {
                cancel_count_reset();
                system_state.state = STATE_FAULT;
                system_state.state_entry_time = now;
                LCD_ShowFault();
                break;
            }

            static bool idle_logged = false;
            if (!idle_logged) {
                Logger_Log(LOG_TIER_B, EVENT_STATE_IDLE, system_state.state);
                idle_logged = true;
            }

            // New-sequence banner: alternate every 5 s between banner and normal display.
            // Only runs when nothing else is pending (count reset confirm suppresses it).
            if (!system_state.count_reset_pending) {
                static bool     prev_new_seq       = false;
                static bool     banner_visible      = false;
                static uint32_t banner_toggle_tick  = 0;

                bool new_seq = SupervisorComms_NewSequenceAvailable();

                if (new_seq && !prev_new_seq) {
                    // Sequence just arrived — show banner immediately
                    banner_visible     = true;
                    banner_toggle_tick = now;
                    LCD_ShowNewSequence(SupervisorComms_GetActiveSeqName());
                } else if (new_seq && (now - banner_toggle_tick >= 5000)) {
                    banner_toggle_tick = now;
                    banner_visible     = !banner_visible;
                    if (banner_visible) {
                        LCD_ShowNewSequence(SupervisorComms_GetActiveSeqName());
                    } else {
                        LCD_ShowIdle(SupervisorComms_GetActiveSeqName(),
                                     SupervisorComms_GetCount(),
                                     SupervisorComms_GetPartNum());
                    }
                }
                prev_new_seq = new_seq;
            }

            // ACK alone: two-step count reset
            if (!inputs.run && handle_count_reset(now, STATE_IDLE)) break;

            // PB1: fetch latest sequence from server
            if (inputs.bypass_rising_edge) {
                cancel_count_reset();
                RequestNewSequence();
            }

            // PB0 + ACK held 1 s: bypass arm
            if (inputs.both_held_1s) {
                cancel_count_reset();
                LEAVE_IDLE();
                Logger_Log(LOG_TIER_A2, EVENT_LOGIN_BYPASS, 0);
                LCD_ShowArmed("Bypass",
                              SupervisorComms_GetCount(), SupervisorComms_GetGoal(),
                              SupervisorComms_GetPartNum());
                SupervisorComms_RequestUpload();
                system_state.state = STATE_ARMED;
                system_state.state_entry_time = now;
            }

            // RFID tap: normal login — show employee ID as placeholder until name resolves
            if (inputs.rfid_rising_edge) {
                cancel_count_reset();
                LEAVE_IDLE();
                Logger_Log(LOG_TIER_A2, EVENT_LOGIN, inputs.rfid_employee_id);
                char emp_str[12];
                snprintf(emp_str, sizeof(emp_str), "%lu", (unsigned long)inputs.rfid_employee_id);
                LCD_ShowArmed(emp_str,
                              SupervisorComms_GetCount(), SupervisorComms_GetGoal(),
                              SupervisorComms_GetPartNum());
                SupervisorComms_LookupEmployee(inputs.rfid_employee_id);
                SupervisorComms_RequestUpload();
                system_state.state = STATE_ARMED;
                system_state.state_entry_time = now;
            }
            break;
        }

        case STATE_ARMED:
            if (!safety_ok) {
                cancel_count_reset();
                system_state.state = STATE_FAULT;
                system_state.state_entry_time = now;
                break;
            }

            // ACK: two-step count reset
            if (handle_count_reset(now, STATE_ARMED)) break;

            // PB0: start sequence
            if (inputs.run_rising_edge) {
                cancel_count_reset();
                system_state.state = STATE_RUNNING;
                Logger_Log(LOG_TIER_A3, EVENT_SEQUENCE_START, now);
                system_state.state_entry_time = now;
                system_state.sequence_active = true;
                system_state.current_step = 0;
                system_state.step_start_time = now;
                LCD_ShowRunning(SupervisorComms_GetActiveSeqName(),
                                SupervisorComms_GetCount(), SupervisorComms_GetGoal());
            }

            // PB1: logoff back to IDLE
            if (inputs.bypass_rising_edge) {
                cancel_count_reset();
                Logger_Log(LOG_TIER_A2, EVENT_LOGOUT, inputs.rfid_employee_id);
                SupervisorComms_ClearOperatorName();
                SupervisorComms_RequestUpload();
                ENTER_IDLE();
            }
            break;

        case STATE_RUNNING:
            if (!safety_ok) {
                system_state.state = STATE_FAULT;
                system_state.state_entry_time = now;
                system_state.sequence_active = false;
                LCD_ShowFault();
                break;
            }

            if (!system_state.sequence_active) {
                SupervisorComms_IncrementCount();
                if (SupervisorComms_IsGoalReached()) {
                    system_state.state = STATE_GOAL_MET;
                    system_state.state_entry_time = now;
                    LCD_ShowGoalReached(SupervisorComms_GetCount(), SupervisorComms_GetGoal());
                } else {
                    system_state.state = STATE_ARMED;
                    system_state.state_entry_time = now;
                    LCD_ShowArmed(SupervisorComms_GetOperatorName(),
                                  SupervisorComms_GetCount(), SupervisorComms_GetGoal(),
                                  SupervisorComms_GetPartNum());
                }
            }
            break;

        case STATE_GOAL_MET:
            system_state.sequence_active = false;

            if (!safety_ok) {
                system_state.state = STATE_FAULT;
                system_state.state_entry_time = now;
                LCD_ShowFault();
                break;
            }

            // ACK: acknowledge goal — reset count, return to ARMED
            if (inputs.ack_rising_edge) {
                uint16_t prev = SupervisorComms_GetCount();
                SupervisorComms_ResetCount();
                Logger_Log(LOG_TIER_B, EVENT_COUNT_RESET, prev);
                system_state.state = STATE_ARMED;
                system_state.state_entry_time = now;
                LCD_ShowArmed(SupervisorComms_GetOperatorName(),
                              0, SupervisorComms_GetGoal(),
                              SupervisorComms_GetPartNum());
            }
            break;

        case STATE_FAULT:
            system_state.sequence_active = false;

            // E-stop released → automatic fault clear
            if (inputs.estop_release_edge) {
                Logger_Log(LOG_TIER_A1, EVENT_SAFETY_RESET, 0);
                SupervisorComms_RequestUpload();
                FAULT_LATCHED  = false;
                ESTOP_ASSERTED = false;
                Logger_ClearA1Overflow();
                ENTER_IDLE();
            }
            break;
    }
}
