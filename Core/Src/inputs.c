#include "inputs.h"
#include "main.h"
#include "mfrc522.h"

// ACK_BTN_Pin, RUN_Pin, FETCH_Pin defined in main.h

Inputs_t inputs;
static bool rfid_active = false;

// Both-held combo state
static uint32_t both_held_start = 0;
static bool     both_held_fired  = false;

void Inputs_EnableRFID(bool enabled) {
    rfid_active = enabled;
    if (!enabled) inputs.rfid_rising_edge = false;
}

void Inputs_Init(void) {
    // RUN, FETCH, ACK — active-low, pull-up (all on GPIOB)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = RUN_Pin | FETCH_Pin | ACK_BTN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // E-stop: NC contact between 3.3V and PB4 — pull-down holds LOW when contact opens (pressed)
    GPIO_InitStruct.Pin  = ESTOP_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(ESTOP_Port, &GPIO_InitStruct);

    // Manual mode switch inputs on GPIOC — active-high, pull-down (series LED circuit)
    GPIO_InitStruct.Pin  = MAN_SW1_Pin | MAN_SW2_Pin | MAN_SW3_Pin | MAN_SW4_Pin |
                           MAN_SW5_Pin | MAN_SW6_Pin | MAN_SW7_Pin | MAN_SW8_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    inputs = (Inputs_t){0};  // zero all fields
}

void Inputs_Update(void) {
    // ── E-stop ────────────────────────────────────────────────────────────
    bool estop_now          = (HAL_GPIO_ReadPin(ESTOP_Port, ESTOP_Pin) == GPIO_PIN_RESET);
    inputs.estop_release_edge = !estop_now && inputs.estop_last;
    inputs.estop            = estop_now;
    inputs.estop_last       = estop_now;

    // ── RUN button ────────────────────────────────────────────────────────
    bool run_now            = (HAL_GPIO_ReadPin(RUN_Port, RUN_Pin) == GPIO_PIN_RESET);
    inputs.run_rising_edge  = run_now && !inputs.run_last;
    inputs.run              = run_now;
    inputs.run_last         = run_now;

    // ── FETCH button ──────────────────────────────────────────────────────
    bool bypass_now            = (HAL_GPIO_ReadPin(FETCH_Port, FETCH_Pin) == GPIO_PIN_RESET);
    inputs.bypass_rising_edge  = bypass_now && !inputs.bypass_last;
    inputs.bypass              = bypass_now;
    inputs.bypass_last         = bypass_now;

    // ── ACK button ────────────────────────────────────────────────────────
    bool ack_now            = (HAL_GPIO_ReadPin(ACK_BTN_GPIO_Port, ACK_BTN_Pin) == GPIO_PIN_RESET);
    inputs.ack_rising_edge  = ack_now && !inputs.ack_last;
    inputs.ack              = ack_now;
    inputs.ack_last         = ack_now;

    // ── PB1 + ACK combo held 1 s → bypass arm ────────────────────────────
    inputs.bypass_ack_held_1s = false;
    if (bypass_now && ack_now) {
        if (!both_held_fired) {
            if (both_held_start == 0) {
                both_held_start = HAL_GetTick();
            } else if (HAL_GetTick() - both_held_start >= 1000) {
                inputs.bypass_ack_held_1s = true;
                both_held_fired           = true;
            }
        }
    } else {
        both_held_start = 0;
        both_held_fired = false;
    }

    // ── PB0 + PB1 combo held 2 s → manual/jog mode entry ────────────────────
    static uint32_t pb_held_start = 0;
    static bool     pb_held_fired  = false;
    inputs.both_pb_held_2s = false;
    if (run_now && bypass_now) {
        if (!pb_held_fired) {
            if (pb_held_start == 0) {
                pb_held_start = HAL_GetTick();
            } else if (HAL_GetTick() - pb_held_start >= 2000) {
                inputs.both_pb_held_2s = true;
                pb_held_fired          = true;
            }
        }
    } else {
        pb_held_start = 0;
        pb_held_fired = false;
    }

    // ── Manual mode switches (always read; state machine gates use) ──────────
    static GPIO_TypeDef *sw_ports[8] = {
        MAN_SW1_Port, MAN_SW2_Port, MAN_SW3_Port, MAN_SW4_Port,
        MAN_SW5_Port, MAN_SW6_Port, MAN_SW7_Port, MAN_SW8_Port
    };
    static uint16_t sw_pins[8] = {
        MAN_SW1_Pin, MAN_SW2_Pin, MAN_SW3_Pin, MAN_SW4_Pin,
        MAN_SW5_Pin, MAN_SW6_Pin, MAN_SW7_Pin, MAN_SW8_Pin
    };
    for (int i = 0; i < NUM_RELAYS; i++) {
        inputs.manual_sw[i] = (HAL_GPIO_ReadPin(sw_ports[i], sw_pins[i]) == GPIO_PIN_SET);
    }

    // ── RFID (IDLE-only, gated by rfid_active) ────────────────────────────
    inputs.rfid_rising_edge = false;
    static uint32_t rfid_poll_tick = 0;
    if (rfid_active && HAL_GetTick() - rfid_poll_tick >= 50) {
        rfid_poll_tick = HAL_GetTick();
        uint32_t id = MFRC522_ReadEmployeeID();
        if (id != 0) {
            inputs.rfid_employee_id = id;
            inputs.rfid_rising_edge = true;
        }
    }
}
