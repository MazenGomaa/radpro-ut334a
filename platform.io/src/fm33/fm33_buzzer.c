/*
 * Rad Pro
 * FM33 buzzer
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 */

#if defined(FM33)

#if defined(BUZZER)

#include "../fm33/device.h"
#include "../peripherals/buzzer.h"

// PWM on GPTIM0 CH1 into the 8002A amplifier, as stock (0x6CCC, 0x6C6C): volume = duty cycle
static const uint16_t buzzerVolumeOnTimes[] = {
    BUZZER_TIMER_PERIOD / 128,
    BUZZER_TIMER_PERIOD / 32,
    BUZZER_TIMER_PERIOD / 8,
    BUZZER_TIMER_PERIOD / 2,
};

static uint16_t buzzerOnTime = BUZZER_TIMER_PERIOD / 2;

void initBuzzer(void)
{
    set_bits(RCC->PCLKCR4, RCC_PCLKCR4_GT0_PCE_Msk);

    BUZZER_TIMER->CR1 = 0;
    BUZZER_TIMER->PSC = BUZZER_TIMER_PRESCALER - 1;
    BUZZER_TIMER->ARR = BUZZER_TIMER_PERIOD - 1;
    BUZZER_TIMER->CCMR1 = (0b110 << GPTIM_CCMR1_OC1M_Pos) | GPTIM_CCMR1_OC1PE_Msk;
    BUZZER_TIMER->CCR1 = 0;
    BUZZER_TIMER->CCER = GPTIM_CCER_CC1E_Msk;
    BUZZER_TIMER->EGR = GPTIM_EGR_UG_Msk;
    BUZZER_TIMER->CR1 = GPTIM_CR1_ARPE_Msk | GPTIM_CR1_CEN_Msk;

    gpio_setup_af_dfs(BUZZER_PORT, BUZZER_PIN, GPIO_OUTPUTTYPE_PUSHPULL, GPIO_PULL_FLOATING, BUZZER_DFS);
}

void setBuzzerVolume(uint8_t volume)
{
    buzzerOnTime = buzzerVolumeOnTimes[volume];
}

void setBuzzer(bool value)
{
    BUZZER_TIMER->CCR1 = value ? buzzerOnTime : 0;
}

#endif

#endif
