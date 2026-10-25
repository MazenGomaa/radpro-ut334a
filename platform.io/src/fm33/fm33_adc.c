/*
 * Rad Pro
 * FM33 ADC
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 *
 * Battery: PC8 = ADC_IN9 against the internal VREF channel, with the factory VREF calibration (measured at
 * VDDA = 3.0 V). ADC set up as the stock firmware does (0x6938, 0x6950, 0x55E0, 0x56DC): RCHF / 8 = 1 MHz,
 * 16x hardware oversampling. The battery divider hangs off HVbatt, so PC4 must be on while sampling.
 * No charger status pin reaches the MCU: "charging" is inferred from USB power and the voltage, as stock.
 */

#if defined(FM33)

#include "../fm33/device.h"
#include "../peripherals/adc.h"
#include "../system/power.h"

#define VREF_CAL_ADDRESS 0x1FFFFB08
#define VREF_CAL_VDDA 3.0F
#define VREF_CAL_DEFAULT 1638 // 1.2 V at 3.0 V
#define ADC_VALUE_MAX ((1 << 12) - 1)

#define PWR_BAT_SAMPLE_NUM 4
#define VREF_SAMPLE_NUM 4

static float batteryVoltage;

static uint32_t getVREFCal(void)
{
    uint32_t value = *(const uint32_t *)VREF_CAL_ADDRESS;

    // Valid if the upper half is the complement of the lower half
    if (((value >> 16) & 0xffff) == (~value & 0xffff))
        return value & 0xffff;

    return VREF_CAL_DEFAULT;
}

void initADC(void)
{
    // GPIO
    set_bits(PWR_BAT_PORT->ANEN, 1U << PWR_BAT_PIN);
    gpio_setup_analog(PWR_BAT_PORT, PWR_BAT_PIN);

    // RCC: ADC clock from RCHF / 8
    set_bits(RCC->PCLKCR2, RCC_PCLKCR2_ADC_PCE_Msk);
    set_bits(RCC->PCLKCR1, RCC_PCLKCR1_ANAC_PCE_Msk);
    modify_bits(RCC->OPCCR2,
                RCC_OPCCR2_ADCCKE_Msk | RCC_OPCCR2_ADCCKS_Msk | RCC_OPCCR2_ADCPRSC_Msk,
                RCC_OPCCR2_ADCCKE_Msk | (0b01 << RCC_OPCCR2_ADCCKS_Pos) | (0b011 << RCC_OPCCR2_ADCPRSC_Pos));

    // ADC: single conversion, 16x oversampling (shift 4), stock sampling times
    ADC->CR = 0;
    ADC->CFGR = (0x4 << ADC_CFGR_OVSS_Pos) |
                (0x3 << ADC_CFGR_OVSR_Pos) |
                ADC_CFGR_OVSEN_Msk |
                ADC_CFGR_WAIT_Msk |
                ADC_CFGR_OVRM_Msk;
    ADC->SMTR = (0xb << ADC_SMTR_CHCG_Pos) | (0x7 << ADC_SMTR_SMTS2_Pos) | (0x7 << ADC_SMTR_SMTS1_Pos);
}

void onADCTick(uint32_t index)
{
}

static uint32_t convertADC(uint32_t channelMask, uint32_t sampleNum)
{
    ADC->CHER = channelMask;
    ADC->CR = ADC_CR_ADEN_Msk;

    uint32_t value = 0;
    for (uint32_t i = 0; i < sampleNum; i++)
    {
        ADC->ISR = ADC_ISR_EOC_Msk;
        set_bits(ADC->CR, ADC_CR_START_Msk);
        wait_until_bits_set(ADC->ISR, ADC_ISR_EOC_Msk);
        value += ADC->DR & ADC_DR_DATA_Msk;
    }

    ADC->ISR = ADC_ISR_EOC_Msk;
    ADC->CR = 0;
    ADC->CHER = 0;

    return value;
}

float readBatteryVoltage(void)
{
    bool railWasOff = !gpio_get_output(PWR_HVBATT_PORT, PWR_HVBATT_PIN);
    if (railWasOff)
    {
        gpio_set(PWR_HVBATT_PORT, PWR_HVBATT_PIN);
        fm33ExtUsleep(PWR_BAT_SETTLE_US);
    }

    uint32_t battery = convertADC(ADC_CHER_ECH9_Msk, PWR_BAT_SAMPLE_NUM);

    if (railWasOff)
        gpio_clear(PWR_HVBATT_PORT, PWR_HVBATT_PIN);

    // VREF channel through the VREF buffer (stock 0x56DC)
    VREF->BUFCR = VREF_BUFCR_VREFBUFFER_EN_Msk;
    uint32_t vref = convertADC(ADC_CHER_REFCH_Msk, VREF_SAMPLE_NUM);
    VREF->BUFCR = 0;

    if (!vref)
        return 0.0F;

    batteryVoltage = (VREF_CAL_VDDA / ADC_VALUE_MAX * PWR_BAT_SCALE_FACTOR * VREF_SAMPLE_NUM / PWR_BAT_SAMPLE_NUM) *
                     getVREFCal() * battery / vref;

    return batteryVoltage;
}

bool isBatteryCharging(void)
{
    return isUSBPowered() && (batteryVoltage < PWR_BAT_CHARGING_VOLTAGE);
}

#endif
