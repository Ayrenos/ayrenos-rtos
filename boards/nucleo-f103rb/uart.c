#include "uart.h"
#include "board.h"
#include <stddef.h>

#define RCC_APB2ENR (*(volatile uint32_t *)0x40021018u)
#define RCC_APB1ENR (*(volatile uint32_t *)0x4002101Cu)
#define GPIOA_CRL   (*(volatile uint32_t *)0x40010800u)
#define USART2_SR  (*(volatile uint32_t *)0x40004400u)
#define USART2_DR  (*(volatile uint32_t *)0x40004404u)
#define USART2_BRR (*(volatile uint32_t *)0x40004408u)
#define USART2_CR1 (*(volatile uint32_t *)0x4000440Cu)
#define USART2_CR2 (*(volatile uint32_t *)0x40004410u)
#define USART2_CR3 (*(volatile uint32_t *)0x40004414u)

#define GPIOA_CLOCK_ENABLE  (1u << 2)
#define USART2_CLOCK_ENABLE (1u << 17)
#define USART_RXNE          (1u << 5)
#define USART_TXE           (1u << 7)
#define USART_RX_ERRORS     0xFu /* Parity, framing, noise, overrun. */
#define UART_BAUD           115200U

void uart_init(void)
{
    RCC_APB2ENR |= GPIOA_CLOCK_ENABLE;
    RCC_APB1ENR |= USART2_CLOCK_ENABLE;
    (void)RCC_APB2ENR;
    (void)RCC_APB1ENR;

    /* PA2: alternate-function push-pull, 2 MHz; PA3: floating input. */
    uint32_t config = GPIOA_CRL;
    config &= ~((0xFu << 8) | (0xFu << 12));
    config |= (0xAu << 8) | (0x4u << 12);
    GPIOA_CRL = config;

    USART2_CR1 = 0;
    USART2_CR2 = 0;
    USART2_CR3 = 0;
    /* APB1 runs at the core clock with the reset clock configuration.
     * With 16x oversampling, BRR encodes the rounded clock/baud ratio.
     */
    USART2_BRR = (BOARD_CORE_CLOCK_HZ + UART_BAUD / 2U) / UART_BAUD;
    USART2_CR1 = (1u << 13) | (1u << 3) | (1u << 2); /* UE, TE, RE */
}

void uart_putc(char character)
{
    while ((USART2_SR & USART_TXE) == 0U) {
    }

    USART2_DR = (uint8_t)character;
}

void uart_write(const char *text)
{
    while (*text != '\0') {
        uart_putc(*text++);
    }
}

bool uart_try_read(uint8_t *byte)
{
    if (byte == NULL) {
        return false;
    }

    uint32_t status = USART2_SR;

    if ((status & (USART_RXNE | USART_RX_ERRORS)) == 0U) {
        return false;
    }

    /* Reading SR then DR clears RXNE and receive errors. Drop errored data. */
    uint8_t data = (uint8_t)USART2_DR;

    if ((status & USART_RX_ERRORS) != 0U) {
        return false;
    }

    *byte = data;
    return true;
}
