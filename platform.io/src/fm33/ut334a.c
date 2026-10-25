/*
 * Rad Pro
 * UNI-T UT334A driver
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 */

#if defined(UT334A)

#include "../fm33/device.h"
#include "../peripherals/comm.h"
#include "../peripherals/display.h"
#include "../peripherals/keyboard.h"
#include "../system/system.h"

// System

void initGPIO(void)
{
    rcc_enable_pad();

    // Safe state before anything else (reset leaves every pin a disconnected input, so the HV
    // MOSFET gate on PC5 would float): HV converter stopped, HVbatt rail off, LCD supply off,
    // LED and speaker off. PD7/PD8 stay SWD.
    gpio_clear(TUBE_HV_PORT, TUBE_HV_PIN);
    gpio_setup_output(TUBE_HV_PORT, TUBE_HV_PIN, GPIO_OUTPUTTYPE_PUSHPULL);
    gpio_clear(PWR_HVBATT_PORT, PWR_HVBATT_PIN);
    gpio_setup_output(PWR_HVBATT_PORT, PWR_HVBATT_PIN, GPIO_OUTPUTTYPE_PUSHPULL);
    gpio_set(DISPLAY_POWER_PORT, DISPLAY_POWER_PIN);
    gpio_setup_output(DISPLAY_POWER_PORT, DISPLAY_POWER_PIN, GPIO_OUTPUTTYPE_PUSHPULL);
    gpio_clear(PULSE_LED_PORT, PULSE_LED_PIN);
    gpio_setup_output(PULSE_LED_PORT, PULSE_LED_PIN, GPIO_OUTPUTTYPE_PUSHPULL);
    gpio_clear(ALERT_LED_PORT, ALERT_LED_PIN);
    gpio_setup_output(ALERT_LED_PORT, ALERT_LED_PIN, GPIO_OUTPUTTYPE_PUSHPULL);
    gpio_clear(BUZZER_PORT, BUZZER_PIN);
    gpio_setup_output(BUZZER_PORT, BUZZER_PIN, GPIO_OUTPUTTYPE_PUSHPULL);

    // Keys (needed early: boot() checks the power key)
    gpio_setup_input(KEY_LEFT_PORT, KEY_LEFT_PIN, GPIO_PULL_PULLUP);
    gpio_setup_input(KEY_RIGHT_PORT, KEY_RIGHT_PIN, GPIO_PULL_PULLUP);
    gpio_setup_input(KEY_OK_PORT, KEY_OK_PIN, GPIO_PULL_PULLUP);
}

void initSystem(void)
{
    initSystemClock();

    rcc_enable_rtc();
}

// Comm

const char *const commId = "UNI-T UT334A;" FIRMWARE_NAME " " FIRMWARE_VERSION "/" LANGUAGE;

// Keyboard

void initKeyboardHardware(void)
{
}

void onKeyboardTick(void)
{
}

void updateKeyboardState(void)
{
}

// Display

void initDisplay(void)
{
}

void setDisplayEnabled(bool value)
{
}

bool isDisplayEnabled(void)
{
    return false;
}

void refreshDisplay(void)
{
}

#endif
