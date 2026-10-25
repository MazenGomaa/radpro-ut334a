/*
 * Rad Pro
 * UNI-T UT334A driver
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 *
 * Pins and values: ut334a/docs/ut334a-hardware.md
 */

#include "fm33-ext.h"

#define SYSCLK_FREQUENCY 64000000
#define AHB_FREQUENCY (SYSCLK_FREQUENCY / 1)
#define APB_FREQUENCY (AHB_FREQUENCY / 1)
#define XTLF_FREQUENCY 32768
#define XTLF_OUT_PIN 9 // PD9
#define XTLF_IN_PIN 10 // PD10

// PD7 (SWCLK) and PD8 (SWDIO) stay SWD: never reconfigure them

#define PWR_USB_PORT GPIOA
#define PWR_USB_PIN 9
#define PWR_USB_EXTI 2 // EXTI[2] = PA8..PA11, ASEL[5:4] = 01 selects PA9
#define PWR_USB_EXTI_ASEL 0b01
#define PWR_HVBATT_PORT GPIOC
#define PWR_HVBATT_PIN 4
#define PWR_BAT_PORT GPIOC
#define PWR_BAT_PIN 8
#define PWR_BAT_CHANNEL 9
#define PWR_BAT_SCALE_FACTOR (1.0F / 0.5438F) // divider measured on the bench (stock firmware: 0.55)
#define PWR_BAT_SETTLE_US 5000 // divider is on HVbatt: settle time after switching PC4 on (to verify)
#define PWR_BAT_CHARGING_VOLTAGE 4.1F // stock: USB powered and below 4.1 V = charging

#define TUBE_HV_PORT GPIOC
#define TUBE_HV_PIN 5
// HV: PC5 gates the LM3478 (high = run). Stock: on 1 tick in 5 of its 204.8 Hz tick (4.9 ms every 24.4 ms,
// 41 Hz, 20 %), plus on after each GM pulse until its next tick (0..4.9 ms). At our 1 kHz tick:
#define TUBE_HV_PERIOD_TICKS 24
#define TUBE_HV_ON_TICKS 5
#define TUBE_HV_PULSE_TICKS 3
// GM pulses: PA8 = COMP2_INP1 vs VREF/2 = 0.6 V, digital filter, rising edge
#define TUBE_DET_PORT GPIOA
#define TUBE_DET_PIN 8
#define TUBE_DET_COMP COMP2
#define TUBE_DET_IRQ COMPx_IRQn
#define TUBE_DET_IRQ_HANDLER COMPx_IRQHandler
#define TUBE_DET_TIMER BSTIM32

#define KEY_LEFT_PORT GPIOA
#define KEY_LEFT_PIN 15
#define KEY_LEFT_WKUP 0
#define KEY_RIGHT_PORT GPIOD
#define KEY_RIGHT_PIN 11
#define KEY_RIGHT_WKUP 6
#define KEY_OK_PORT GPIOA
#define KEY_OK_PIN 10
#define KEY_OK_WKUP 1

#define DISPLAY_RESX_PORT GPIOB
#define DISPLAY_RESX_PIN 13
#define DISPLAY_CSX_PORT GPIOB
#define DISPLAY_CSX_PIN 3
#define DISPLAY_DCX_PORT GPIOB
#define DISPLAY_DCX_PIN 8
#define DISPLAY_SCL_PORT GPIOB
#define DISPLAY_SCL_PIN 9
#define DISPLAY_SDA_PORT GPIOB
#define DISPLAY_SDA_PIN 11
#define DISPLAY_SPI SPI1
#define DISPLAY_POWER_PORT GPIOB
#define DISPLAY_POWER_PIN 14
#define DISPLAY_POWER_ACTIVE_LOW
#define DISPLAY_BACKLIGHT_PORT GPIOB
#define DISPLAY_BACKLIGHT_PIN 2
#define DISPLAY_BACKLIGHT_DFS true // PB2 second digital function = ATIM_CH1N
#define DISPLAY_BACKLIGHT_ACTIVE_LOW // lit while PB2 is low (bench)
#define DISPLAY_BACKLIGHT_TIMER ATIM
// Stock: PSC = 1, ARR = 9999 -> 3.2 kHz
#define DISPLAY_BACKLIGHT_TIMER_PRESCALER 2
#define DISPLAY_BACKLIGHT_TIMER_PERIOD 10000

#define BUZZER_PORT GPIOB
#define BUZZER_PIN 10
#define BUZZER_DFS true // PB10 second digital function = GPTIM0_CH1
#define BUZZER_TIMER GPTIM0
// Stock tone: PSC = 2, ARR = 13399 -> 1.592 kHz (8002A amplifier and speaker)
#define BUZZER_TIMER_PRESCALER 3
#define BUZZER_TIMER_PERIOD 13400

// Bi-colour LED between PC2 and PC3: PC3 high = colour A (stock: pulses), PC2 high = colour B (stock: alarm)
#define PULSE_LED_PORT GPIOC
#define PULSE_LED_PIN 3
#define ALERT_LED_PORT GPIOC
#define ALERT_LED_PIN 2

#define USART_INTERFACE UART0
#define USART_RX_PORT GPIOA
#define USART_RX_PIN 13
#define USART_TX_PORT GPIOA
#define USART_TX_PIN 14
#define USART_IRQ UART0_IRQn
#define USART_IRQ_HANDLER UART0_IRQHandler
