/*
 * Rad Pro
 * FM33 RTC
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 */

#if defined(FM33)

#include "../peripherals/rtc.h"
#include "../fm33/device.h"

void initRTC(void)
{
}

bool setDeviceTime(uint32_t value)
{
    return false;
}

uint32_t getDeviceTime(void)
{
    return 0;
}

uint32_t getDeviceTimeFast(void)
{
    return 0;
}

#endif
