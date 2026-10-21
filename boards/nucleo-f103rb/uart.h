#ifndef AYRENOS_UART_H
#define AYRENOS_UART_H

#include <stdbool.h>
#include <stdint.h>

void uart_init(void);
void uart_putc(char character);
void uart_write(const char *text);

bool uart_try_read(uint8_t *byte);

#endif
