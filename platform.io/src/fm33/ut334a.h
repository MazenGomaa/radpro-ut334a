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
#define PWR_BAT_SCALE_FACTOR (1.0F / 0.5438F)

#define TUBE_HV_PORT GPIOC
#define TUBE_HV_PIN 5
#define TUBE_DET_PORT GPIOA
#define TUBE_DET_PIN 8

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
#define DISPLAY_BACKLIGHT_TIMER ATIM

#define BUZZER_PORT GPIOB
#define BUZZER_PIN 10
#define BUZZER_TIMER GPTIM0

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
