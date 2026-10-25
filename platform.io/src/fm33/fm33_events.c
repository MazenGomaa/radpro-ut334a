/*
 * Rad Pro
 * FM33 events
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 */

#if defined(FM33)

#include "../system/events.h"
#include "../fm33/device.h"

void initEvents(void)
{
}

void SysTick_Handler(void)
{
    currentTick++;

    onTick();
}

void reloadWatchdog(void)
{
}

void sleep(uint32_t value)
{
}

#endif
