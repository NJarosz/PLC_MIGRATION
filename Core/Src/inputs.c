#include "inputs.h"
#include "main.h"
#include "mfrc522.h"

Inputs_t inputs;
static bool rfid_active = false;
static uint32_t bypass_press_start = 0;
static bool bypass_hold_fired = false;

void Inputs_EnableRFID(bool enabled) {
    rfid_active = enabled;
    if (!enabled) inputs.rfid_rising_edge = false;
}

void Inputs_Init(void) {
    /* PB0 and PB1 are active-low inputs — CubeMX leaves them NOPULL, fix here */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    inputs.estop             = false;
    inputs.run               = false;
    inputs.run_last          = false;
    inputs.run_rising_edge   = false;
    inputs.bypass               = false;
    inputs.bypass_last          = false;
    inputs.bypass_rising_edge   = false;
    inputs.bypass_short_release = false;
    inputs.bypass_hold_2s       = false;
    inputs.rfid_rising_edge  = false;
    inputs.rfid_employee_id  = 0;
}

void Inputs_Update(void) {
    inputs.estop = (HAL_GPIO_ReadPin(ESTOP_Port, ESTOP_Pin) == GPIO_PIN_RESET);

    inputs.run             = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET);
    inputs.run_rising_edge = inputs.run && !inputs.run_last;
    inputs.run_last        = inputs.run;

    bool bypass_now = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET);

    // Detect rising edge first so hold timer starts this same cycle
    if (bypass_now && !inputs.bypass_last) {
        bypass_press_start = HAL_GetTick();
        bypass_hold_fired  = false;
    }

    inputs.bypass_rising_edge   = bypass_now && !inputs.bypass_last;
    inputs.bypass_short_release = !bypass_now && inputs.bypass_last && !bypass_hold_fired;
    inputs.bypass_hold_2s       = false;

    if (bypass_now && !bypass_hold_fired && (HAL_GetTick() - bypass_press_start >= 2000)) {
        inputs.bypass_hold_2s = true;
        bypass_hold_fired     = true;
    }

    inputs.bypass      = bypass_now;
    inputs.bypass_last = bypass_now;

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
