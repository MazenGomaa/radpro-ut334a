/*
 * Rad Pro
 * FM33 comm
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 *
 * Serial port on UART0 (PA13 RX, PA14 TX: DFS = 0), 115200 8N1 as the other Rad Pro serial devices. UART0
 * runs from APBCLK (UART0CKS = 00): baud = APBCLK / (SPBRG + 1). The UT334A has no USB data connection: an
 * external USB-serial adapter connects here. Rad Pro opens the port while USB power is present.
 */

#if defined(FM33)

#include "../peripherals/comm.h"
#include "../fm33/device.h"
#include "../system/cstring.h"

#define COMM_SERIAL_BAUDRATE 115200

void initCommHardware(void)
{
}

void openComm(void)
{
    if (comm.open)
        return;

    // RCC
    set_bits(RCC->PCLKCR3, RCC_PCLKCR3_UART0_PCE_Msk);
    modify_bits(RCC->OPCCR1,
                RCC_OPCCR1_UART0CKE_Msk | RCC_OPCCR1_UART0CKS_Msk,
                RCC_OPCCR1_UART0CKE_Msk);

    // GPIO: RX pulled up, so an unconnected adapter reads as idle
    gpio_setup_af_dfs(USART_RX_PORT, USART_RX_PIN, GPIO_OUTPUTTYPE_PUSHPULL, GPIO_PULL_PULLUP, false);
    gpio_setup_af_dfs(USART_TX_PORT, USART_TX_PIN, GPIO_OUTPUTTYPE_PUSHPULL, GPIO_PULL_FLOATING, false);

    // UART: 8 data bits (PDSEL = 01), no parity, 1 stop bit
    USART_INTERFACE->BGR = (APB_FREQUENCY + COMM_SERIAL_BAUDRATE / 2) / COMM_SERIAL_BAUDRATE - 1;
    USART_INTERFACE->CSR = (0b01 << UART_CSR_PDSEL_Pos) | UART_CSR_RXEN_Msk | UART_CSR_TXEN_Msk;
    USART_INTERFACE->ISR = UART_ISR_OERR_Msk | UART_ISR_FERR_Msk | UART_ISR_PERR_Msk;
    USART_INTERFACE->IER = UART_IER_RXBF_IE_Msk;

    NVIC_SetPriority(USART_IRQ, 0x40);
    NVIC_EnableIRQ(USART_IRQ);

    clearComm(true);
}

void closeComm(void)
{
    if (!comm.open)
        return;

    // UART
    NVIC_DisableIRQ(USART_IRQ);

    USART_INTERFACE->IER = 0;
    USART_INTERFACE->CSR = 0;

    // GPIO
    gpio_setup_analog(USART_RX_PORT, USART_RX_PIN);
    gpio_setup_analog(USART_TX_PORT, USART_TX_PIN);

    // RCC
    clear_bits(RCC->OPCCR1, RCC_OPCCR1_UART0CKE_Msk);
    clear_bits(RCC->PCLKCR3, RCC_PCLKCR3_UART0_PCE_Msk);

    clearComm(false);
}

void transmitComm(void)
{
    comm.state = COMM_TX;

    set_bits(USART_INTERFACE->IER, UART_IER_TXBE_IE_Msk);
}

void USART_IRQ_HANDLER(void)
{
    int16_t c = -1;

    uint32_t isr = USART_INTERFACE->ISR;

    if (isr & UART_ISR_RXBF_Msk)
        c = USART_INTERFACE->RXBUF & 0xff;

    if (isr & (UART_ISR_OERR_Msk | UART_ISR_FERR_Msk | UART_ISR_PERR_Msk))
    {
        // Write 1 to clear
        USART_INTERFACE->ISR = UART_ISR_OERR_Msk | UART_ISR_FERR_Msk | UART_ISR_PERR_Msk;

        if (comm.state == COMM_RX)
        {
            // Invalidate command
            comm.buffer[0] = ' ';
            comm.bufferIndex = 1;
        }
    }

    switch (comm.state)
    {
    case COMM_RX:
        if ((c >= ' ') &&
            (comm.bufferIndex < (COMM_BUFFER_SIZE - 1)))
            comm.buffer[comm.bufferIndex++] = c;
        else if ((c == '\r') ||
                 ((c == '\n') &&
                  (comm.previousChar != '\r')))
        {
            comm.buffer[comm.bufferIndex] = '\0';

            comm.bufferIndex = 0;
            comm.state = COMM_RX_READY;
        }

        comm.previousChar = c;

        break;

    case COMM_TX:
        if (isr & UART_ISR_TXBE_Msk)
        {
            c = comm.buffer[comm.bufferIndex++];
            if (c != '\0')
                USART_INTERFACE->TXBUF = c;
            else
            {
                clear_bits(USART_INTERFACE->IER, UART_IER_TXBE_IE_Msk);

                strclr(comm.buffer);
                comm.bufferIndex = 0;

                if (comm.transmitState == TRANSMIT_RESPONSE)
                    comm.state = COMM_RX;
                else
                    comm.state = COMM_TX_READY;
            }
        }

        break;

    default:
        break;
    }
}

void pollComm(void)
{
}

#endif
