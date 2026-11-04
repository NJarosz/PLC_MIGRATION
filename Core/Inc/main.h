/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define B1_Pin GPIO_PIN_13
#define B1_GPIO_Port GPIOC
#define USART_TX_Pin GPIO_PIN_2
#define USART_TX_GPIO_Port GPIOA
#define USART_RX_Pin GPIO_PIN_3
#define USART_RX_GPIO_Port GPIOA
#define LD2_Pin GPIO_PIN_5
#define LD2_GPIO_Port GPIOA
#define RFID_CS_Pin GPIO_PIN_12
#define RFID_CS_GPIO_Port GPIOB
#define RFID_RST_Pin GPIO_PIN_8
#define RFID_RST_GPIO_Port GPIOC
#define TMS_Pin GPIO_PIN_13
#define TMS_GPIO_Port GPIOA
#define TCK_Pin GPIO_PIN_14
#define TCK_GPIO_Port GPIOA
#define SWO_Pin GPIO_PIN_3
#define SWO_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

// ── Relay outputs — all on GPIOA, active-high into ULN2803A ──────────────
// PA0-PA1: relays 1-2; PA4-PA8: relays 3-7 (PA2/PA3=UART, PA9/PA10=UART1);
// PA11: relay 8. On Nucleo bench: PA5 also drives LD2 (green LED) — useful
// as a visual indicator for relay 4 during testing.
#define RELAY1_Pin GPIO_PIN_0
#define RELAY1_Port GPIOA

#define RELAY2_Pin GPIO_PIN_1
#define RELAY2_Port GPIOA

#define RELAY3_Pin GPIO_PIN_4
#define RELAY3_Port GPIOA

#define RELAY4_Pin GPIO_PIN_5
#define RELAY4_Port GPIOA

#define RELAY5_Pin GPIO_PIN_6
#define RELAY5_Port GPIOA

#define RELAY6_Pin GPIO_PIN_7
#define RELAY6_Port GPIOA

#define RELAY7_Pin GPIO_PIN_8
#define RELAY7_Port GPIOA

#define RELAY8_Pin GPIO_PIN_11
#define RELAY8_Port GPIOA

#define NUM_RELAYS 8

// ── Panel button inputs — active-low, pull-up (GPIOB) ────────────────────
#define RUN_Pin          GPIO_PIN_0
#define RUN_Port         GPIOB
#define FETCH_Pin        GPIO_PIN_1
#define FETCH_Port       GPIOB
#define ACK_BTN_Pin      GPIO_PIN_10
#define ACK_BTN_GPIO_Port GPIOB

#define ESTOP_Pin        GPIO_PIN_4
#define ESTOP_Port       GPIOB

// ── External cycle-start / hand button inputs — active-low, pull-up ──────
#define BTN_CYC_L_Pin    GPIO_PIN_14
#define BTN_CYC_L_Port   GPIOB
#define BTN_CYC_R_Pin    GPIO_PIN_15
#define BTN_CYC_R_Port   GPIOB
#define BTN_CYC_3_Pin    GPIO_PIN_11
#define BTN_CYC_3_Port   GPIOC

// ── Manual mode switch inputs — active-high, pull-down, PC3-PC10 ─────────
// Consecutive 8-bit block: sw_states = (GPIOC->IDR >> 3) & 0xFF
// Series LED circuit in pendant: 3.3V → switch → GPIO → LED → 220Ω → GND.
#define MAN_SW1_Pin   GPIO_PIN_3
#define MAN_SW1_Port  GPIOC
#define MAN_SW2_Pin   GPIO_PIN_4
#define MAN_SW2_Port  GPIOC
#define MAN_SW3_Pin   GPIO_PIN_5
#define MAN_SW3_Port  GPIOC
#define MAN_SW4_Pin   GPIO_PIN_6
#define MAN_SW4_Port  GPIOC
#define MAN_SW5_Pin   GPIO_PIN_7
#define MAN_SW5_Port  GPIOC
#define MAN_SW6_Pin   GPIO_PIN_8
#define MAN_SW6_Port  GPIOC
#define MAN_SW7_Pin   GPIO_PIN_9
#define MAN_SW7_Port  GPIOC
#define MAN_SW8_Pin   GPIO_PIN_10
#define MAN_SW8_Port  GPIOC

// ── RFID SPI ──────────────────────────────────────────────────────────────
#define RFID_CS_Pin    GPIO_PIN_12
#define RFID_CS_Port   GPIOB
#define RFID_RST_Pin   GPIO_PIN_12
#define RFID_RST_Port  GPIOA

// ── ESP32 UART ────────────────────────────────────────────────────────────
#define ESP_TX_Pin GPIO_PIN_9
#define ESP_TX_GPIO_Port GPIOA
#define ESP_RX_Pin GPIO_PIN_10
#define ESP_RX_GPIO_Port GPIOA

// ── HMI board LED drive outputs — active-high, push-pull (GPIOB) ─────────
#define LED_ARMED_Pin   GPIO_PIN_5
#define LED_ARMED_Port  GPIOB
#define LED_RUN_Pin     GPIO_PIN_7
#define LED_RUN_Port    GPIOB
#define LED_FAULT_Pin   GPIO_PIN_6
#define LED_FAULT_Port  GPIOB

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
