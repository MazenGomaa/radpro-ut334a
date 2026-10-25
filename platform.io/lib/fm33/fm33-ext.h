/*
 * FM33 CMSIS extensions
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 *
 * Register bits: fm33lc0xx_bits.h (generated from the SVD), checked against the FM33LC0xx datasheet V1.9.
 */

#ifndef _FM33_EXT_H_
#define _FM33_EXT_H_

#include <stdbool.h>
#include <stdint.h>

#include "fm33.h"
#include "fm33lc0xx_bits.h"

#define MAX_SYSCLK 64000000

__STATIC_INLINE void fm33ExtUsleep(uint32_t value)
{
    for (volatile uint32_t i = 0; i < (uint32_t)(value * (MAX_SYSCLK / 1000000)); i++)
        ;
}

// RCC

__STATIC_INLINE void rcc_enable_pad(void)
{
    set_bits(RCC->PCLKCR1, RCC_PCLKCR1_PAD_PCE_Msk);
}

__STATIC_INLINE void rcc_enable_rtc(void)
{
    set_bits(RCC->PCLKCR1, RCC_PCLKCR1_RTC_PCE_Msk);
}

__STATIC_INLINE void rcc_enable_iwdt(void)
{
    set_bits(RCC->PCLKCR1, RCC_PCLKCR1_IWDT_PCE_Msk);
}

// EXTI runs from LSCLK so it can wake the chip from DeepSleep (datasheet 34.6.2)
__STATIC_INLINE void rcc_enable_exti(void)
{
    set_bits(RCC->OPCCR1, RCC_OPCCR1_EXTICKE_Msk | RCC_OPCCR1_EXTICKS_Msk);
}

__STATIC_INLINE void rcc_enable_spi1(void)
{
    set_bits(RCC->PCLKCR3, RCC_PCLKCR3_SPI1_PCE_Msk);
}

// ATIM kernel clock: APBCLK (ATCKS = 0), as in the stock firmware
__STATIC_INLINE void rcc_enable_atim(void)
{
    set_bits(RCC->PCLKCR4, RCC_PCLKCR4_AT_PCE_Msk);
    modify_bits(RCC->OPCCR1, RCC_OPCCR1_ATCKE_Msk | RCC_OPCCR1_ATCKS_Msk, RCC_OPCCR1_ATCKE_Msk);
}

__STATIC_INLINE void rcc_disable_atim(void)
{
    clear_bits(RCC->OPCCR1, RCC_OPCCR1_ATCKE_Msk);
    clear_bits(RCC->PCLKCR4, RCC_PCLKCR4_AT_PCE_Msk);
}

// GPIO

#define GPIO_PULL_FLOATING 0
#define GPIO_PULL_PULLUP 1

#define GPIO_OUTPUTTYPE_PUSHPULL 0
#define GPIO_OUTPUTTYPE_OPENDRAIN 1

// GPIOx_FCR (datasheet 34.8.4)
#define GPIO_FUNCTION_INPUT 0b00
#define GPIO_FUNCTION_OUTPUT 0b01
#define GPIO_FUNCTION_DIGITAL 0b10
#define GPIO_FUNCTION_ANALOG 0b11

__STATIC_INLINE void gpio_modify_bit(volatile uint32_t *reg, uint8_t pin, bool value)
{
    if (value)
        set_bits(*reg, 1U << pin);
    else
        clear_bits(*reg, 1U << pin);
}

__STATIC_INLINE void gpio_setup(GPIO_Type *base, uint8_t pin, uint8_t function, bool input, uint8_t output_type, uint8_t pull)
{
    gpio_modify_bit(&base->INEN, pin, input);
    gpio_modify_bit(&base->PUEN, pin, pull == GPIO_PULL_PULLUP);
    gpio_modify_bit(&base->ODEN, pin, output_type == GPIO_OUTPUTTYPE_OPENDRAIN);
    modify_bits(base->FCR, 0b11U << (2 * pin), (uint32_t)function << (2 * pin));
}

__STATIC_INLINE void gpio_setup_input(GPIO_Type *base, uint8_t pin, uint8_t pull)
{
    gpio_setup(base, pin, GPIO_FUNCTION_INPUT, true, GPIO_OUTPUTTYPE_PUSHPULL, pull);
}

__STATIC_INLINE void gpio_setup_output(GPIO_Type *base, uint8_t pin, uint8_t output_type)
{
    gpio_setup(base, pin, GPIO_FUNCTION_OUTPUT, false, output_type, GPIO_PULL_FLOATING);
}

__STATIC_INLINE void gpio_setup_af(GPIO_Type *base, uint8_t pin, uint8_t output_type, uint8_t pull)
{
    gpio_setup(base, pin, GPIO_FUNCTION_DIGITAL, true, output_type, pull);
}

__STATIC_INLINE void gpio_setup_analog(GPIO_Type *base, uint8_t pin)
{
    gpio_setup(base, pin, GPIO_FUNCTION_ANALOG, false, GPIO_OUTPUTTYPE_PUSHPULL, GPIO_PULL_FLOATING);
}

// GPIOx_DFS: second digital function of a pin (datasheet 34.8.9)
__STATIC_INLINE void gpio_setup_af_dfs(GPIO_Type *base, uint8_t pin, uint8_t output_type, uint8_t pull, bool dfs)
{
    gpio_modify_bit(&base->DFS, pin, dfs);
    gpio_setup_af(base, pin, output_type, pull);
}

__STATIC_INLINE bool gpio_get(const GPIO_Type *base, uint8_t pin)
{
    return (base->DIN >> pin) & 1;
}

__STATIC_INLINE bool gpio_get_output(const GPIO_Type *base, uint8_t pin)
{
    return (base->DO >> pin) & 1;
}

__STATIC_INLINE void gpio_set(GPIO_Type *base, uint8_t pin)
{
    base->DSET = 1U << pin;
}

__STATIC_INLINE void gpio_clear(GPIO_Type *base, uint8_t pin)
{
    base->DRST = 1U << pin;
}

__STATIC_INLINE void gpio_modify(GPIO_Type *base, uint8_t pin, bool value)
{
    if (value)
        gpio_set(base, pin);
    else
        gpio_clear(base, pin);
}

// WKUP pins (datasheet 34.5, 34.8.17): wake from Sleep/DeepSleep, IRQ #38 (WKUP_IRQn) when WKISEL = 1

__STATIC_INLINE void gpio_enable_wakeup(uint8_t wkup, bool rising_edge)
{
    gpio_modify_bit(&GPIO->PINWKEN, GPIO_COMMON_PINWKEN_SEL0_Pos + wkup, rising_edge);
    set_bits(GPIO->PINWKEN, GPIO_COMMON_PINWKEN_WKISEL_Msk | (1U << (GPIO_COMMON_PINWKEN_EN0_Pos + wkup)));
}

__STATIC_INLINE void gpio_disable_wakeup(uint8_t wkup)
{
    clear_bits(GPIO->PINWKEN, 1U << (GPIO_COMMON_PINWKEN_EN0_Pos + wkup));
}

// SPI (datasheet 22): master, transmit only, 8-bit frames, CPOL = CPHA = 0, MSB first

#define SPI_BAUD_DIV2 0b000

__STATIC_INLINE void spi_setup_master_tx(SPI_Type *base, uint32_t baud)
{
    base->CR2 = 0;
    base->CR1 = SPI_CR1_MM_Msk | (baud << SPI_CR1_BAUD_Pos);
    base->CR3 = SPI_CR3_TXBFC_Msk;
    base->CR2 = SPI_CR2_TXO_Msk | SPI_CR2_SPIEN_Msk;
}

__STATIC_INLINE void spi_send(SPI_Type *base, uint32_t value)
{
    wait_until_bits_set(base->ISR, SPI_ISR_TXBE_Msk);
    base->TXBUF = value;
}

__STATIC_INLINE void spi_wait_while_busy(SPI_Type *base)
{
    wait_until_bits_set(base->ISR, SPI_ISR_TXBE_Msk);
    wait_until_bits_clear(base->ISR, SPI_ISR_BUSY_Msk);
}

// IWDT (datasheet 9.6)

#define IWDT_OVP_4S 0b101
#define IWDT_SERV_KEY 0x12345A5A

// Once started, the IWDT cannot be stopped. With the factory OPTBYTES (IWDTSLP = 0xF) it also keeps
// counting in Sleep/DeepSleep, so sleeping code must wake up to reload it.
__STATIC_INLINE void iwdt_start(uint32_t ovp)
{
    rcc_enable_iwdt();
    IWDT->CR = ovp << IWDT_CR_OVP_Pos;
    IWDT->SERV = IWDT_SERV_KEY;
}

__STATIC_INLINE void iwdt_reload(void)
{
    IWDT->SERV = IWDT_SERV_KEY;
}

#endif
