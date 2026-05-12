#include "state_machine.h"
#include "main.h"
#include "inputs.h"
#include "logger.h"
#include "sequence_engine.h"
#include "safety.h"
#include "supervisor_comms.h"
#include "lcd.h"

#define ENTER_IDLE()  do { system_state.state = STATE_IDLE;  system_state.state_entry_time = now; Inputs_EnableRFID(true);  LCD_ShowIdle();   } while(0)
#define LEAVE_IDLE()  do {                                                                          Inputs_EnableRFID(false);                   } while(0)


System_t system_state;

void StateMachine_Init(void) {
    system_state.state = STATE_BOOT;
    system_state.state_entry_time = HAL_GetTick();
    system_state.sequence_active = false;
    system_state.current_step = 0;
    system_state.step_start_time = 0;
    system_state.fault_request = false;
}

void StateMachine_Update(bool safety_ok) {
    uint32_t now = HAL_GetTick();

    // A1 buffer overflow is a hard fault — machine must halt until operator reset
    if (Logger_A1_Overflowed() && system_state.state != STATE_FAULT) {
        Logger_Log(LOG_TIER_B, EVENT_A1_OVERFLOW, 0);  // LOG_TIER_B: A1 is full, use general buffer
        system_state.fault_request = true;
    }

    if (system_state.fault_request) {
        system_state.state = STATE_FAULT;
        system_state.state_entry_time = now;
        system_state.sequence_active = false;
        system_state.fault_request = false;
        LCD_ShowFault();
    }

    switch (system_state.state) {
        case STATE_BOOT:
            Logger_Log(LOG_TIER_B, EVENT_STATE_BOOT, system_state.state);
            ENTER_IDLE();
            Logger_Log(LOG_TIER_B, EVENT_BOOT_COMPLETE, now);
            break;

        case STATE_IDLE:
            if (!safety_ok) {
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

            // PB0: request latest sequence from server
            if (inputs.run_rising_edge) {
                RequestNewSequence();
            }

            // RFID tap: normal login
            if (inputs.rfid_rising_edge) {
                LEAVE_IDLE();
                Logger_Log(LOG_TIER_A2, EVENT_LOGIN, inputs.rfid_employee_id);
                LCD_ShowArmed("", SupervisorComms_GetCount(), SupervisorComms_GetGoal());
                SupervisorComms_LookupEmployee(inputs.rfid_employee_id);
                SupervisorComms_RequestUpload();
                system_state.state = STATE_ARMED;
                system_state.state_entry_time = now;
            }

            // PB1: bypass arm (lost card / bypass switch in production)
            if (inputs.bypass_short_release) {
                LEAVE_IDLE();
                Logger_Log(LOG_TIER_A2, EVENT_LOGIN_BYPASS, 0);
                LCD_ShowArmed("", SupervisorComms_GetCount(), SupervisorComms_GetGoal());
                SupervisorComms_RequestUpload();
                system_state.state = STATE_ARMED;
                system_state.state_entry_time = now;
            }
            break;

        case STATE_ARMED:
            if (!safety_ok) {
                system_state.state = STATE_FAULT;
                system_state.state_entry_time = now;
                break;
            }

            // PB0: start sequence
            if (inputs.run_rising_edge) {
                system_state.state = STATE_RUNNING;
                Logger_Log(LOG_TIER_A3, EVENT_SEQUENCE_START, now);
                system_state.state_entry_time = now;
                system_state.sequence_active = true;
                system_state.current_step = 0;
                system_state.step_start_time = now;
                LCD_ShowRunning(SupervisorComms_GetActiveSeqName(),
                                SupervisorComms_GetCount(), SupervisorComms_GetGoal());
            }

            // PB1 short tap: logoff back to IDLE
            if (inputs.bypass_short_release) {
                Logger_Log(LOG_TIER_A2, EVENT_LOGOUT, inputs.rfid_employee_id);
                SupervisorComms_ClearOperatorName();
                SupervisorComms_RequestUpload();
                ENTER_IDLE();
            }

            // PB1 hold 2s: reset run count without logging off
            if (inputs.bypass_hold_2s) {
                uint16_t prev = SupervisorComms_GetCount();
                SupervisorComms_ResetCount();
                Logger_Log(LOG_TIER_B, EVENT_COUNT_RESET, prev);
                LCD_ShowArmed(SupervisorComms_GetOperatorName(), 0, SupervisorComms_GetGoal());
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
                                  SupervisorComms_GetCount(), SupervisorComms_GetGoal());
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

            // Short press: acknowledge goal, reset count, return to ARMED
            if (inputs.bypass_short_release) {
                uint16_t prev = SupervisorComms_GetCount();
                SupervisorComms_ResetCount();
                Logger_Log(LOG_TIER_B, EVENT_COUNT_RESET, prev);
                system_state.state = STATE_ARMED;
                system_state.state_entry_time = now;
                LCD_ShowArmed(SupervisorComms_GetOperatorName(), 0, SupervisorComms_GetGoal());
            }
            break;

        case STATE_FAULT:
            system_state.sequence_active = false;

            bool reset_pressed = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET);
            bool estop_released = !inputs.estop;  // Assuming active-low; adjust if active-high

            if (reset_pressed && estop_released) {
                Logger_Log(LOG_TIER_A1, EVENT_SAFETY_RESET, 0);
                SupervisorComms_RequestUpload();
                FAULT_LATCHED = false;
                ESTOP_ASSERTED = false;
                Logger_ClearA1Overflow();  // allow A1 overflow check to re-arm after reset
                ENTER_IDLE();
            }
            break;
    }
}
