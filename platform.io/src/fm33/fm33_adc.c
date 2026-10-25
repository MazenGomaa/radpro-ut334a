/*
 * Rad Pro
 * FM33 ADC
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 */

#if defined(FM33)

#include "../peripherals/adc.h"
#include "../fm33/device.h"

void initADC(void)
{
}

void onADCTick(uint32_t index)
{
}

float readBatteryVoltage(void)
{
    return 4.0F;
}

#endif
