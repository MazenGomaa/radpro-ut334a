/*
 * Rad Pro
 * FM33 power
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 *
 * The UT334A has no power latch: "off" is DeepSleep with RAM and RTC kept, as in the stock firmware.
 * setPowerEnabled(false) only allows the main loop to enter DeepSleep (sleepWhilePoweredOff()). Wake sources: the three keys (WKUP pins,
 * falling edge), USB power (PA9, EXTI rising edge) and the RTC 1 s interrupt, needed because the IWDT keeps
 * counting in DeepSleep with the factory option bytes.
 */

#if defined(FM33)

#include "../fm33/device.h"
#include "../peripherals/adc.h"
#include "../peripherals/led.h"
#include "../system/power.h"
#include "../system/events.h"

static bool powerEnabled;

void initPower(bool value)
{
    setPowerEnabled(value);

    gpio_setup_input(PWR_USB_PORT, PWR_USB_PIN, GPIO_PULL_FLOATING);

    // PWR_USB EXTI
    rcc_enable_exti();
    modify_bits(GPIO->EXTISEL,
                0b11U << (GPIO_COMMON_EXTISEL_ASEL_Pos + 2 * PWR_USB_EXTI),
                PWR_USB_EXTI_ASEL << (GPIO_COMMON_EXTISEL_ASEL_Pos + 2 * PWR_USB_EXTI));
    set_bits(GPIO->EXTIDF, 1U << PWR_USB_EXTI);
}

void setPowerEnabled(bool value)
{
    powerEnabled = value;
}

bool isUSBPowered(void)
{
    return gpio_get(PWR_USB_PORT, PWR_USB_PIN);
}

bool wasResetByWatchdog(void)
{
    return get_bits(RCC->RSTFR, RCC_RSTFR_IWDTN_FLAG_Msk);
}

void clearResetFlags(void)
{
    // Write 1 to clear
    RCC->RSTFR = RCC->RSTFR;
}

// DeepSleep

static bool isAnyKeyDown(void)
{
    return !gpio_get(KEY_LEFT_PORT, KEY_LEFT_PIN) ||
           !gpio_get(KEY_RIGHT_PORT, KEY_RIGHT_PIN) ||
           !gpio_get(KEY_OK_PORT, KEY_OK_PIN);
}

static bool canEnterDeepSleep(void)
{
    return !powerEnabled && !isUSBPowered() && !isAnyKeyDown();
}

void WKUP_IRQHandler(void)
{
    // WKPxF: write 1 to clear
    PMU->WKFR = 0xFF;
}

void GPIO_IRQHandler(void)
{
    GPIO->EXTIISR = 1U << PWR_USB_EXTI;
}

static void setDeepSleepWakeup(bool value)
{
    if (value)
    {
        // Keys
        PMU->WKFR = 0xFF;
        gpio_enable_wakeup(KEY_LEFT_WKUP, false);
        gpio_enable_wakeup(KEY_RIGHT_WKUP, false);
        gpio_enable_wakeup(KEY_OK_WKUP, false);
        NVIC_ClearPendingIRQ(WKUP_IRQn);
        NVIC_EnableIRQ(WKUP_IRQn);

        // USB power: EXTI rising edge
        GPIO->EXTIISR = 1U << PWR_USB_EXTI;
        modify_bits(GPIO->EXTIEDS, 0b11U << (2 * PWR_USB_EXTI), 0b00U << (2 * PWR_USB_EXTI));
        NVIC_ClearPendingIRQ(GPIO_IRQn);
        NVIC_EnableIRQ(GPIO_IRQn);

        // RTC 1 s
        RTC->ISR = RTC_ISR_SEC_IF_Msk;
        set_bits(RTC->IER, RTC_IER_SEC_IE_Msk);
        NVIC_ClearPendingIRQ(RTC_IRQn);
        NVIC_EnableIRQ(RTC_IRQn);
    }
    else
    {
        NVIC_DisableIRQ(WKUP_IRQn);
        gpio_disable_wakeup(KEY_LEFT_WKUP);
        gpio_disable_wakeup(KEY_RIGHT_WKUP);
        gpio_disable_wakeup(KEY_OK_WKUP);

        NVIC_DisableIRQ(GPIO_IRQn);
        modify_bits(GPIO->EXTIEDS, 0b11U << (2 * PWR_USB_EXTI), 0b11U << (2 * PWR_USB_EXTI));

        NVIC_DisableIRQ(RTC_IRQn);
        clear_bits(RTC->IER, RTC_IER_SEC_IE_Msk);
    }
}

void RTC_IRQHandler(void)
{
    RTC->ISR = RTC_ISR_SEC_IF_Msk;
}

static void enterDeepSleep(void)
{
    setDeepSleepWakeup(true);

    // Datasheet 4.2.6: SLPDP (VREF off) + PMOD = Sleep, SLEEPDEEP must stay clear, then WFI
    PMU->CR = PMU_CR_SLPDP_Msk | (0b10 << PMU_CR_PMOD_Pos);
    __DSB();
    __WFI();
    PMU->CR = 0;

    // Woken up on RCHF 8 MHz with the PLL off
    startSystemPLL();

    setDeepSleepWakeup(false);
}

// Charging indicator while off, as stock (0x26F4): pulse LED colour on while isBatteryCharging() (USB power,
// below the threshold, with the stock hold time). Rad Pro owns the LED while powered on.
#define CHARGING_CHECK_TICKS (5 * SYSTICK_FREQUENCY)

static bool chargingLED;
static uint32_t chargingCheckTick;

static void updateChargingLED(void)
{
    bool value = false;
    if (!isPoweredOn() && isUSBPowered())
    {
        if ((currentTick - chargingCheckTick) >= CHARGING_CHECK_TICKS)
        {
            chargingCheckTick = currentTick;

            readBatteryVoltage();
        }

        value = isBatteryCharging();
    }
    else
        chargingCheckTick = currentTick - CHARGING_CHECK_TICKS;

    if (value != chargingLED)
    {
        chargingLED = value;

        setPulseLED(value);
    }
}

// Called by the main loop after all events are handled. SysTick stops in DeepSleep; the RTC 1 s
// wake-up only reloads the IWDT. Before DeepSleep the LCD supply is cut as in the stock firmware.
void sleepWhilePoweredOff(void)
{
    updateChargingLED();

    if (!canEnterDeepSleep())
        return;

    parkDisplay();

    while (canEnterDeepSleep())
    {
        iwdt_reload();

        enterDeepSleep();
    }
}

#endif
