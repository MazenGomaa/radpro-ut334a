/*
 * Rad Pro
 * FM33 display backlight
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 *
 * PWM on ATIM CH1N as in the stock firmware (0x6A1C, 0x69C4). The backlight is lit while PB2 is low
 * (bench, 2026-10-25: CCR1 0 brightest, 10000 dark with PWM mode 2), so OC1M = PWM mode 1: OC1REF is high
 * while CNT < CCR1 and the complementary output CH1N is low then, so the on-time is CCR1. ATIM outputs also
 * need BDTR.MOE.
 */

#if defined(FM33)

#include "../fm33/device.h"
#include "../peripherals/display.h"
#include "../system/settings.h"

// Gamma-corrected linear brightness values:
// [0.25, 0.5, 0.75, 1] ^ 2.2
static const uint32_t displayOnTime[] = {
    (uint32_t)(0.0473661F * DISPLAY_BACKLIGHT_TIMER_PERIOD),
    (uint32_t)(0.2176376F * DISPLAY_BACKLIGHT_TIMER_PERIOD),
    (uint32_t)(0.5310492F * DISPLAY_BACKLIGHT_TIMER_PERIOD),
    (uint32_t)(DISPLAY_BACKLIGHT_TIMER_PERIOD),
};

void initBacklight(void)
{
    setBacklight(false);
}

void setBacklight(bool value)
{
    if (value)
    {
        rcc_enable_atim();

        DISPLAY_BACKLIGHT_TIMER->CR1 = 0;
        DISPLAY_BACKLIGHT_TIMER->PSC = DISPLAY_BACKLIGHT_TIMER_PRESCALER - 1;
        DISPLAY_BACKLIGHT_TIMER->ARR = DISPLAY_BACKLIGHT_TIMER_PERIOD - 1;
        DISPLAY_BACKLIGHT_TIMER->CCMR1 = (0b110 << ATIM_CCMR1_OC1M_Pos) | ATIM_CCMR1_OC1PE_Msk;
        DISPLAY_BACKLIGHT_TIMER->CCR1 = displayOnTime[settings.displayBrightness];
        DISPLAY_BACKLIGHT_TIMER->CCER = ATIM_CCER_CC1NE_Msk;
        DISPLAY_BACKLIGHT_TIMER->EGR = ATIM_EGR_UG_Msk;
        DISPLAY_BACKLIGHT_TIMER->BDTR = ATIM_BDTR_MOE_Msk;
        DISPLAY_BACKLIGHT_TIMER->CR1 = ATIM_CR1_ARPE_Msk | ATIM_CR1_CEN_Msk;

        gpio_setup_af_dfs(DISPLAY_BACKLIGHT_PORT, DISPLAY_BACKLIGHT_PIN, GPIO_OUTPUTTYPE_PUSHPULL, GPIO_PULL_FLOATING, DISPLAY_BACKLIGHT_DFS);
    }
    else
    {
#if defined(DISPLAY_BACKLIGHT_ACTIVE_LOW)
        gpio_set(DISPLAY_BACKLIGHT_PORT, DISPLAY_BACKLIGHT_PIN);
#else
        gpio_clear(DISPLAY_BACKLIGHT_PORT, DISPLAY_BACKLIGHT_PIN);
#endif
        gpio_setup_output(DISPLAY_BACKLIGHT_PORT, DISPLAY_BACKLIGHT_PIN, GPIO_OUTPUTTYPE_PUSHPULL);

        DISPLAY_BACKLIGHT_TIMER->BDTR = 0;
        DISPLAY_BACKLIGHT_TIMER->CR1 = 0;
        rcc_disable_atim();
    }
}

#endif
