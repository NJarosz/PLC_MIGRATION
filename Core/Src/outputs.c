#include "outputs.h"
#include "main.h"
#include "state_machine.h"

Outputs_t outputs;

void Outputs_Init(void) {
    GPIO_InitTypeDef cfg = {0};
    cfg.Mode  = GPIO_MODE_OUTPUT_PP;
    cfg.Pull  = GPIO_NOPULL;
    cfg.Speed = GPIO_SPEED_FREQ_LOW;

    // All 8 relay outputs on GPIOA — initialize here regardless of CubeMX config
    cfg.Pin = RELAY1_Pin | RELAY2_Pin | RELAY3_Pin | RELAY4_Pin |
              RELAY5_Pin | RELAY6_Pin | RELAY7_Pin | RELAY8_Pin;
    HAL_GPIO_Init(GPIOA, &cfg);
    // De-energize all relays (active-low relay module: HIGH = off)
    HAL_GPIO_WritePin(GPIOA, cfg.Pin, GPIO_PIN_SET);

    // LED outputs — all on GPIOB
    cfg.Pin = LED_ARMED_Pin | LED_RUN_Pin | LED_FAULT_Pin;
    HAL_GPIO_Init(LED_ARMED_Port, &cfg);
    HAL_GPIO_WritePin(LED_ARMED_Port, cfg.Pin, GPIO_PIN_RESET);

    for (int i = 0; i < NUM_RELAYS; i++) {
        outputs.relay_requested[i] = false;
        outputs.relay_actual[i] = false;
    }
}

void Outputs_Apply(bool safety_ok) {
    GPIO_TypeDef* relay_ports[NUM_RELAYS] = {
        RELAY1_Port, RELAY2_Port, RELAY3_Port, RELAY4_Port,
        RELAY5_Port, RELAY6_Port, RELAY7_Port, RELAY8_Port
    };
    uint16_t relay_pins[NUM_RELAYS] = {
        RELAY1_Pin, RELAY2_Pin, RELAY3_Pin, RELAY4_Pin,
        RELAY5_Pin, RELAY6_Pin, RELAY7_Pin, RELAY8_Pin
    };

    for (int i = 0; i < NUM_RELAYS; i++) {
        outputs.relay_actual[i] = safety_ok ? outputs.relay_requested[i] : false;
        // Active-low relay module: RESET = energize, SET = de-energize
        HAL_GPIO_WritePin(relay_ports[i], relay_pins[i],
                          outputs.relay_actual[i] ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }
}

void Outputs_UpdateLEDs(SystemState_t state) {
    HAL_GPIO_WritePin(LED_FAULT_Port, LED_FAULT_Pin,
        state == STATE_FAULT ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_RUN_Port, LED_RUN_Pin,
        (state == STATE_RUNNING || state == STATE_MANUAL) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_ARMED_Port, LED_ARMED_Pin,
        (state == STATE_IDLE || state == STATE_ARMED || state == STATE_GOAL_MET) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
