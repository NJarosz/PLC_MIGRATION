#include "inputs.h"
#include "main.h"

Inputs_t inputs;

// Mock employee IDs — replace with real RFID reader output when hardware is wired.
// Each rising edge on the RFID input cycles to the next ID so all three can be tested
// without physically swapping tags.
static const uint32_t MOCK_EMPLOYEE_IDS[] = {1001, 1002, 1003};
static uint8_t mock_employee_idx = 0;

void Inputs_Init(void) {
    inputs.estop             = false;
    inputs.run               = false;
    inputs.run_last          = false;
    inputs.run_rising_edge   = false;
    inputs.rfid              = false;
    inputs.rfid_last         = false;
    inputs.rfid_rising_edge  = false;
    inputs.rfid_employee_id  = 0;
}

void Inputs_Update(void) {
    inputs.estop = (HAL_GPIO_ReadPin(ESTOP_Port, ESTOP_Pin) == GPIO_PIN_RESET);

    inputs.run             = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET);
    inputs.run_rising_edge = inputs.run && !inputs.run_last;

    inputs.rfid            = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET);
    inputs.rfid_rising_edge = inputs.rfid && !inputs.rfid_last;

    if (inputs.rfid_rising_edge) {
        // TODO: replace with actual RFID reader output (UART/SPI) when hardware is wired
        inputs.rfid_employee_id = MOCK_EMPLOYEE_IDS[mock_employee_idx];
        mock_employee_idx = (mock_employee_idx + 1) % 3;
    }

    inputs.run_last  = inputs.run;
    inputs.rfid_last = inputs.rfid;
}
