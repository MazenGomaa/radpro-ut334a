/*
 * Rad Pro
 * FM33 LED
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 */

#if defined(FM33)

#include "../fm33/device.h"
#include "../peripherals/led.h"

// Bi-colour LED (anti-parallel pair between the two pins): it lights only when exactly one side is
// high. LED_MULTIPLEX keeps pulse and alert from being on at the same time.

#if defined(PULSE_LED)

void initPulseLED(void)
{
    setPulseLED(false);

    gpio_setup_output(PULSE_LED_PORT, PULSE_LED_PIN, GPIO_OUTPUTTYPE_PUSHPULL);
}

void setPulseLED(bool value)
{
    gpio_modify(PULSE_LED_PORT, PULSE_LED_PIN, value);
}

#endif

#if defined(ALERT_LED)

void initAlertLED(void)
{
    setAlertLED(false);

    gpio_setup_output(ALERT_LED_PORT, ALERT_LED_PIN, GPIO_OUTPUTTYPE_PUSHPULL);
}

void setAlertLED(bool value)
{
    gpio_modify(ALERT_LED_PORT, ALERT_LED_PIN, value);
}

#endif

#endif
