/*
 * Rad Pro
 * FM33 power
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 */

#if defined(FM33)

#include "../system/power.h"
#include "../fm33/device.h"

void initPower(bool value)
{
}

void setPowerEnabled(bool value)
{
}

bool isUSBPowered(void)
{
    return false;
}

bool isBatteryCharging(void)
{
    return false;
}

bool wasResetByWatchdog(void)
{
    return false;
}

void clearResetFlags(void)
{
}

#endif
