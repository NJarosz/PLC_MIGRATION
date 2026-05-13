#include "inputs.h"
#include "main.h"
#include "mfrc522.h"

// Fallback pin defines — overridden by CubeMX-generated main.h if the pin
// is labelled "ACK_BTN" in the .ioc file (right-click PB10 → user label).
#ifndef ACK_BTN_Pin
#define ACK_BTN_Pin        GPIO_PIN_10
#define ACK_BTN_GPIO_Port  GPIOB
#endif

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
    // PB0, PB1, PB10 are active-low — CubeMX leaves them NOPULL, fix here
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = GPIO_PIN_0 | GPIO_PIN_1 | ACK_BTN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    inputs = (Inputs_t){0};  // zero all fields
}

void Inputs_Update(void) {
    // ── E-stop ────────────────────────────────────────────────────────────
    bool estop_now          = (HAL_GPIO_ReadPin(ESTOP_Port, ESTOP_Pin) == GPIO_PIN_RESET);
    inputs.estop_release_edge = !estop_now && inputs.estop_last;
    inputs.estop            = estop_now;
    inputs.estop_last       = estop_now;

    // ── PB0 — Run ─────────────────────────────────────────────────────────
    bool run_now            = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET);
    inputs.run_rising_edge  = run_now && !inputs.run_last;
    inputs.run              = run_now;
    inputs.run_last         = run_now;

    // ── PB1 — Fetch/Logoff ────────────────────────────────────────────────
    bool bypass_now            = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET);
    inputs.bypass_rising_edge  = bypass_now && !inputs.bypass_last;
    inputs.bypass              = bypass_now;
    inputs.bypass_last         = bypass_now;

    // ── ACK button ────────────────────────────────────────────────────────
    bool ack_now            = (HAL_GPIO_ReadPin(ACK_BTN_GPIO_Port, ACK_BTN_Pin) == GPIO_PIN_RESET);
    inputs.ack_rising_edge  = ack_now && !inputs.ack_last;
    inputs.ack              = ack_now;
    inputs.ack_last         = ack_now;

    // ── PB0 + ACK combo held 1 s → bypass arm ────────────────────────────
    inputs.both_held_1s = false;
    if (run_now && ack_now) {
        if (!both_held_fired) {
            if (both_held_start == 0) {
                both_held_start = HAL_GetTick();
            } else if (HAL_GetTick() - both_held_start >= 1000) {
                inputs.both_held_1s = true;
                both_held_fired     = true;
            }
        }
    } else {
        both_held_start = 0;
        both_held_fired = false;
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
