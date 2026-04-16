#include "outputs.h"
#include "main.h"

Outputs_t outputs;

void Outputs_Init(void) {
    for (int i = 0; i < NUM_RELAYS; i++) {
        outputs.relay_requested[i] = false;
        outputs.relay_actual[i] = false;
    }
}

void Outputs_Apply(bool safety_ok) {
    GPIO_TypeDef* relay_ports[NUM_RELAYS] = {RELAY1_Port, RELAY2_Port, RELAY3_Port, RELAY4_Port};
    uint16_t relay_pins[NUM_RELAYS] = {RELAY1_Pin, RELAY2_Pin, RELAY3_Pin, RELAY4_Pin};

    for (int i = 0; i < NUM_RELAYS; i++) {
        outputs.relay_actual[i] = safety_ok ? outputs.relay_requested[i] : false;
        HAL_GPIO_WritePin(relay_ports[i], relay_pins[i], outputs.relay_actual[i] ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}
