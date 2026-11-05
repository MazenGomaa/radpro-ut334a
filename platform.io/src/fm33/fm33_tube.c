/*
 * Rad Pro
 * FM33 tube
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 *
 * HV: no timer drives PC5, so the stock burst pattern is generated from SysTick (onTubeHVTick()).
 * Pulses: COMP2 rising-edge interrupt, timestamped by BSTIM32 running free at 1 MHz.
 */

#if defined(FM33)

#include "../fm33/device.h"
#include "../peripherals/tube.h"
#include "../system/events.h"

#define TUBE_BITS_PER_PULSE 2
#define TUBE_BITS_PER_PULSE_MASK ((1 << TUBE_BITS_PER_PULSE) - 1)

struct
{
    volatile bool enabled;

    uint32_t hvPhase;
    volatile uint32_t hvPulseTicks;

    uint32_t previousTick;
    uint32_t previousTimerCount;
} tubeHardware;

void initTubeHardware(void)
{
    // Pulse timer: BSTIM32 on APBCLK, 1 MHz, free running
    set_bits(RCC->PCLKCR4, RCC_PCLKCR4_BT_PCE_Msk);
    modify_bits(RCC->OPCCR2, RCC_OPCCR2_BTCKE_Msk | RCC_OPCCR2_BTCKS_Msk, RCC_OPCCR2_BTCKE_Msk);
    TUBE_DET_TIMER->PSC = APB_FREQUENCY / PULSE_MEASUREMENT_FREQUENCY - 1;
    TUBE_DET_TIMER->ARR = 0xffffffff;
    TUBE_DET_TIMER->EGR = BSTIM32_EGR_UG_Msk;
    TUBE_DET_TIMER->CR1 = BSTIM32_CR1_CEN_Msk;

    // Comparator (stock 0x6A4C): PA8 analog, INP1 vs VREF/2, no inversion
    set_bits(RCC->PCLKCR1, RCC_PCLKCR1_ANAC_PCE_Msk);
    set_bits(TUBE_DET_PORT->ANEN, 1U << TUBE_DET_PIN);
    gpio_setup_analog(TUBE_DET_PORT, TUBE_DET_PIN);
    TUBE_DET_COMP->CR = 0b11 << COMP2_CR_V2NSEL_Pos;
    COMP->ICR = COMP_COMMON_ICR_CMP2DF_Msk |
                (0b01 << COMP_COMMON_ICR_CMP2SEL_Pos) |
                COMP_COMMON_ICR_CMP2IE_Msk;

    NVIC_SetPriority(TUBE_DET_IRQ, 0);
    NVIC_EnableIRQ(TUBE_DET_IRQ);

    setTubeHVEnabled(false);
}

void setTubeHVEnabled(bool value)
{
    if (value)
    {
        // HVbatt rail (LM3478 supply), then comparator
        gpio_set(PWR_HVBATT_PORT, PWR_HVBATT_PIN);
        set_bits(TUBE_DET_COMP->CR, COMP2_CR_CMP2EN_Msk);
        COMP->ISR = COMP_COMMON_ISR_CMP2IF_Msk;

        tubeHardware.enabled = true;
    }
    else
    {
        tubeHardware.enabled = false;

        gpio_clear(TUBE_HV_PORT, TUBE_HV_PIN);
        clear_bits(TUBE_DET_COMP->CR, COMP2_CR_CMP2EN_Msk);
        gpio_clear(PWR_HVBATT_PORT, PWR_HVBATT_PIN);
    }
}

void updateTubeHV(void)
{
}

// Called from SysTick_Handler
void onTubeHVTick(void)
{
    if (!tubeHardware.enabled)
        return;

    tubeHardware.hvPhase++;
    if (tubeHardware.hvPhase >= TUBE_HV_PERIOD_TICKS)
        tubeHardware.hvPhase = 0;

    bool hvOn = (tubeHardware.hvPhase < TUBE_HV_ON_TICKS);
    if (tubeHardware.hvPulseTicks)
    {
        tubeHardware.hvPulseTicks--;
        hvOn = true;
    }

    gpio_modify(TUBE_HV_PORT, TUBE_HV_PIN, hvOn);
}

void TUBE_DET_IRQ_HANDLER(void)
{
    // Write 1 to clear
    COMP->ISR = COMP_COMMON_ISR_CMP2IF_Msk;

    uint32_t timerCount = TUBE_DET_TIMER->CNT;
    uint32_t timerTick = currentTick;

    tubePulseCount++;

    tubeRandomBits = (tubeRandomBits << TUBE_BITS_PER_PULSE) | (timerCount & TUBE_BITS_PER_PULSE_MASK);

    uint32_t pulseIntervalTicks = timerTick - tubeHardware.previousTick;
    if (pulseIntervalTicks < 50)
    {
        uint16_t pulseInterval = timerCount - tubeHardware.previousTimerCount;
        if (pulseInterval < tubeDeadTime)
            tubeDeadTime = pulseInterval;
    }

    tubeHardware.previousTimerCount = timerCount;
    tubeHardware.previousTick = timerTick;

    // Recharge after each pulse, as the stock firmware does
    if (tubeHardware.enabled)
    {
        gpio_set(TUBE_HV_PORT, TUBE_HV_PIN);
        tubeHardware.hvPulseTicks = TUBE_HV_PULSE_TICKS;
    }
}

bool readTubeDet(void)
{
    // Comparator output stuck high: tube shorted
    return get_bits(TUBE_DET_COMP->CR, COMP2_CR_CMP2O_Msk);
}

#endif
