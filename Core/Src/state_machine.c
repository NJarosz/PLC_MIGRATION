#include "state_machine.h"
#include "main.h"
#include "inputs.h"
#include "logger.h"
#include "sequence_engine.h"
#include "safety.h"


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

    if (system_state.fault_request) {
        system_state.state = STATE_FAULT;
        system_state.state_entry_time = now;
        system_state.sequence_active = false;
        system_state.fault_request = false;
    }

    switch (system_state.state) {
        case STATE_BOOT:
            Logger_Log(LOG_TIER_B, EVENT_STATE_BOOT, system_state.state);
            system_state.state = STATE_IDLE;
            system_state.state_entry_time = now;
            Logger_Log(LOG_TIER_B, EVENT_BOOT_COMPLETE, now);
            break;

        case STATE_IDLE:
            if (!safety_ok) {
                system_state.state = STATE_FAULT;
                system_state.state_entry_time = now;
                break;
            }

            static bool idle_logged = false;
            if (!idle_logged) {
                Logger_Log(LOG_TIER_B, EVENT_STATE_IDLE, system_state.state);
                idle_logged = true;
            }

            // PB0 button triggers sequence request while in IDLE
			static bool pb0_last = false;
			bool pb0_pressed = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET);  // assuming active low

			if (pb0_pressed && !pb0_last) {
				RequestNewSequence();        // Ask ESP32 for latest sequence
			}
			pb0_last = pb0_pressed;

            // Arm on RFID rising edge
            if (inputs.rfid_rising_edge) {
                Logger_Log(LOG_TIER_A2, EVENT_LOGIN, system_state.state);
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

            // Start sequence on run rising edge
            if (inputs.run_rising_edge) {
                system_state.state = STATE_RUNNING;
                Logger_Log(LOG_TIER_A3, EVENT_SEQUENCE_START, now);
                system_state.state_entry_time = now;
                system_state.sequence_active = true;
                system_state.current_step = 0;
                system_state.step_start_time = now;
            }

            // Logout on RFID rising edge
            if (inputs.rfid_rising_edge) {
                Logger_Log(LOG_TIER_A2, EVENT_LOGOUT, system_state.state);
                system_state.state = STATE_IDLE;
                system_state.state_entry_time = now;
            }
            break;

        case STATE_RUNNING:
            if (!safety_ok) {
                system_state.state = STATE_FAULT;
                system_state.state_entry_time = now;
                system_state.sequence_active = false;
                break;
            }

            if (!system_state.sequence_active) {
                system_state.state = STATE_ARMED;
                system_state.state_entry_time = now;
            }
            break;

        case STATE_FAULT:
            system_state.sequence_active = false;

            bool reset_pressed = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET);
            bool estop_released = !inputs.estop;  // Assuming active-low; adjust if active-high

            if (reset_pressed && estop_released) {
                printf("FAULT RESET (ESTOP CLEARED)\r\n");  // Debug; remove in production
                Logger_Log(LOG_TIER_A1, EVENT_SAFETY_RESET, 0);  // New event code—add to logger.h enum
                FAULT_LATCHED = false;
                ESTOP_ASSERTED = false;
                system_state.state = STATE_IDLE;
                system_state.state_entry_time = now;
            }
            break;
    }
}
