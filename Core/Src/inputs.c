#include "inputs.h"
#include "main.h"

Inputs_t inputs;

void Inputs_Init(void) {
    // No specific init needed for inputs, as they are polled via HAL_GPIO_ReadPin
    inputs.estop = false;
    inputs.run = false;
    inputs.run_last = false;
    inputs.run_rising_edge = false;
    inputs.rfid = false;
    inputs.rfid_last = false;
    inputs.rfid_rising_edge = false;
}

void Inputs_Update(void) {
    // Update inputs
    inputs.estop = (HAL_GPIO_ReadPin(ESTOP_Port, ESTOP_Pin) == GPIO_PIN_RESET);  // Assuming active low

    inputs.run = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET);  // Simulate run button
    inputs.run_rising_edge = inputs.run && !inputs.run_last;

    // Simulate RFID with another input if needed; for now, using SWO_Pin as placeholder
    inputs.rfid = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET);
    inputs.rfid_rising_edge = inputs.rfid && !inputs.rfid_last;

    // Update last states
    inputs.run_last = inputs.run;
    inputs.rfid_last = inputs.rfid;
}
