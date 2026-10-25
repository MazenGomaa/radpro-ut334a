/*
 * Rad Pro
 * FM33 system
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 */

#if defined(FM33)

#include "../fm33/device.h"
#include "../system/cstring.h"
#include "../system/system.h"

#define UID_BASE 0x1FFFFA10

// Equivalent of the FMSH SystemInit() (system_fm33lc0xx.c, DFP 3.0.4), plus the 32.768 kHz crystal.
// The stock UT334A firmware runs exactly this sequence (emulated: SystemInit 0xC6FC, XTLF 0xD064).
void initSystemClock(void)
{
    // Reset PLL and SYSCLK selection: RCHF, LSCLK auto switch to LPOSC if XTLF fails, EXTI sampling in sleep
    RCC->PLLCR = 0;
    RCC->SYSCLKCR = RCC_SYSCLKCR_LSCATS_Msk | RCC_SYSCLKCR_SLP_ENEXTI_Msk;

    rcc_enable_pad();

    // PDR and BOR
    RMU->PDRCR = 0x1;
    RMU->BORCR = 0xE;

    // Stop IWDT and WWDT while the core is halted by a debugger
    DBG->CR = 0x3;

    // Factory trim values
    RCC->RCHFTR = RCHF8M_TRIM;
    RCC->RCMFTR = RCMF4M_TRIM;
    RCC->LPOSCTR = LPOSC_TRIM;

    // SWD (PD7, PD8) pull-ups
    set_bits(GPIOD->PUEN, (1U << 7) | (1U << 8));

    // XTLF: PD9/PD10 analog, enable, LSCLK from XTLF
    set_bits(GPIOD->ANEN, (1U << XTLF_OUT_PIN) | (1U << XTLF_IN_PIN));
    gpio_setup_analog(GPIOD, XTLF_OUT_PIN);
    gpio_setup_analog(GPIOD, XTLF_IN_PIN);
    RCC->XTLFCR = 1 << RCC_XTLFCR_IPW_Pos;
    RCC->XTLFCR = (1 << RCC_XTLFCR_IPW_Pos) | (0x5 << RCC_XTLFCR_EN_Pos);
    RCC->LSCLKSEL = 0xAA;

    startSystemPLL();
}

// RCHF 8 MHz / 8 = 1 MHz -> PLL x64 = 64 MHz, 2 flash wait states (stock firmware 0x6DD8(64)).
// Also used after waking from DeepSleep, which restarts on RCHF with the PLL off.
void startSystemPLL(void)
{
    RCC->RCHFCR = RCC_RCHFCR_EN_Msk;
    FLASH->RDCR = 2 << FLASH_RDCR_WAIT_Pos;

    RCC->PLLCR = ((SYSCLK_FREQUENCY / 1000000 - 1) << RCC_PLLCR_DB_Pos) |
                 (0b011 << RCC_PLLCR_REFPRSC_Pos);
    set_bits(RCC->PLLCR, RCC_PLLCR_EN_Msk);
    wait_until_bits_set(RCC->PLLCR, RCC_PLLCR_LOCKED_Msk);

    modify_bits(RCC->SYSCLKCR,
                RCC_SYSCLKCR_SYSCLKSEL_Msk | RCC_SYSCLKCR_AHBPRES_Msk | RCC_SYSCLKCR_APBPRES1_Msk | RCC_SYSCLKCR_APBPRES2_Msk,
                0b010 << RCC_SYSCLKCR_SYSCLKSEL_Pos);
}

void getDeviceId(char *s)
{
    *s = '\0';
    for (uint32_t i = 0; i < 4 * 3; i++)
        strcatUInt8Hex(s, ((const uint8_t *)UID_BASE)[i]);
}

#endif
