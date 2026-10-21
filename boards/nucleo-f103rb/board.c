#include "board.h"
#include <stdint.h>

#define RCC_APB2ENR (*(volatile uint32_t *)0x40021018u)
#define GPIOA_CRL  (*(volatile uint32_t *)0x40010800u)
#define GPIOA_BSRR (*(volatile uint32_t *)0x40010810u)

#define GPIOA_CLOCK_ENABLE (1u << 2)
#define LED_PIN            5u

void board_init(void)
{
    RCC_APB2ENR |= GPIOA_CLOCK_ENABLE;

    (void)RCC_APB2ENR;

    GPIOA_BSRR = 1u << (LED_PIN + 16u);

    uint32_t config = GPIOA_CRL;
    config &= ~(0xFu << (LED_PIN * 4u));
    config |=  (0x2u << (LED_PIN * 4u));
    GPIOA_CRL = config;
}
void board_led_write(bool on)
{
    if (on) {
        GPIOA_BSRR = 1u << LED_PIN;
    } else {
        GPIOA_BSRR = 1u << (LED_PIN + 16u);
    }
}