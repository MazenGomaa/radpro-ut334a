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
}

void initSystem(void)
{
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
