/*
 * Rad Pro
 * FM33 startup (vector table and reset handler)
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 */

#if defined(FM33)

#include <stdint.h>

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

int main(void);

void Reset_Handler(void);
void Default_Handler(void);

#define WEAK_HANDLER(name) void name(void) __attribute__((weak, alias("Default_Handler")))

WEAK_HANDLER(NMI_Handler);
WEAK_HANDLER(HardFault_Handler);
WEAK_HANDLER(SVC_Handler);
WEAK_HANDLER(PendSV_Handler);
WEAK_HANDLER(SysTick_Handler);
WEAK_HANDLER(WWDT_IRQHandler);
WEAK_HANDLER(SVD_IRQHandler);
WEAK_HANDLER(RTC_IRQHandler);
WEAK_HANDLER(FLASH_IRQHandler);
WEAK_HANDLER(LFDET_IRQHandler);
WEAK_HANDLER(ADC_IRQHandler);
WEAK_HANDLER(IWDT_IRQHandler);
WEAK_HANDLER(SPI1_IRQHandler);
WEAK_HANDLER(SPI2_IRQHandler);
WEAK_HANDLER(LCD_IRQHandler);
WEAK_HANDLER(UART0_IRQHandler);
WEAK_HANDLER(UART1_IRQHandler);
WEAK_HANDLER(UART4_IRQHandler);
WEAK_HANDLER(UART5_IRQHandler);
WEAK_HANDLER(HFDET_IRQHandler);
WEAK_HANDLER(U7816_IRQHandler);
WEAK_HANDLER(LPUART1_IRQHandler);
WEAK_HANDLER(I2C_IRQHandler);
WEAK_HANDLER(USB_IRQHandler);
WEAK_HANDLER(AES_IRQHandler);
WEAK_HANDLER(LPTIM_IRQHandler);
WEAK_HANDLER(DMA_IRQHandler);
WEAK_HANDLER(WKUP_IRQHandler);
WEAK_HANDLER(OPAx_IRQHandler);
WEAK_HANDLER(BSTIM_IRQHandler);
WEAK_HANDLER(COMPx_IRQHandler);
WEAK_HANDLER(GPTIM0_IRQHandler);
WEAK_HANDLER(GPTIM1_IRQHandler);
WEAK_HANDLER(ATIM_IRQHandler);
WEAK_HANDLER(VREF_IRQHandler);
WEAK_HANDLER(GPIO_IRQHandler);
WEAK_HANDLER(LPUART0_IRQHandler);

// Order from IRQn_Type in fm33lc0xx.h
__attribute__((section(".isr_vector"), used)) static void (*const vectorTable[16 + 32])(void) = {
    (void (*)(void))&_estack,
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    SVC_Handler,
    0,
    0,
    PendSV_Handler,
    SysTick_Handler,

    WWDT_IRQHandler,
    SVD_IRQHandler,
    RTC_IRQHandler,
    FLASH_IRQHandler,
    LFDET_IRQHandler,
    ADC_IRQHandler,
    IWDT_IRQHandler,
    SPI1_IRQHandler,
    SPI2_IRQHandler,
    LCD_IRQHandler,
    UART0_IRQHandler,
    UART1_IRQHandler,
    UART4_IRQHandler,
    UART5_IRQHandler,
    HFDET_IRQHandler,
    U7816_IRQHandler,
    LPUART1_IRQHandler,
    I2C_IRQHandler,
    USB_IRQHandler,
    AES_IRQHandler,
    LPTIM_IRQHandler,
    DMA_IRQHandler,
    WKUP_IRQHandler,
    OPAx_IRQHandler,
    BSTIM_IRQHandler,
    COMPx_IRQHandler,
    GPTIM0_IRQHandler,
    GPTIM1_IRQHandler,
    ATIM_IRQHandler,
    VREF_IRQHandler,
    GPIO_IRQHandler,
    LPUART0_IRQHandler,
};

void Reset_Handler(void)
{
    const uint32_t *source = &_sidata;
    for (uint32_t *dest = &_sdata; dest < &_edata;)
        *dest++ = *source++;

    for (uint32_t *dest = &_sbss; dest < &_ebss;)
        *dest++ = 0;

    main();

    while (1)
        ;
}

void Default_Handler(void)
{
    while (1)
        ;
}

#endif
