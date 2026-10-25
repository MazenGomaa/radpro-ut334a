/*
 * Rad Pro
 * FM33 events
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 */

#if defined(FM33)

#include "../fm33/device.h"
#include "../system/events.h"

void initEvents(void)
{
    // SysTick
    NVIC_SetPriority(SysTick_IRQn, 2);
    SysTick->LOAD = AHB_FREQUENCY / SYSTICK_FREQUENCY - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

    iwdt_start(IWDT_OVP_4S);
}

void SysTick_Handler(void)
{
    currentTick++;

    onTubeHVTick();

    onTick();
}

void reloadWatchdog(void)
{
    iwdt_reload();
}

void sleep(uint32_t value)
{
    uint32_t targetTick = currentTick + value;

    while (true)
    {
        iwdt_reload();

        if (((int32_t)(currentTick - targetTick)) >= 0)
            break;

        __WFI();
    }
}

#endif
