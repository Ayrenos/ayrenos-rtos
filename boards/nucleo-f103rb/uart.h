#ifndef AYRENOS_UART_H
#define AYRENOS_UART_H

#include <stdbool.h>
#include <stdint.h>

/* USART2: PA2 TX, PA3 RX, 115200 baud, 8 data bits, no parity, 1 stop bit.
 * Initialize once before use, with the board's reset clock configuration.
 * Polling only: callers must serialize access if sharing between tasks.
 */
void uart_init(void);
void uart_putc(char character);
void uart_write(const char *text);

/* Returns false when no valid byte is available; leaves *byte unchanged. */
bool uart_try_read(uint8_t *byte);

#endif
