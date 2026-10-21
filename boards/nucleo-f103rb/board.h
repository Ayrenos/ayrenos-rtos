#ifndef AYRENOS_BOARD_H
#define AYRENOS_BOARD_H

#include <stdbool.h>

#define BOARD_CORE_CLOCK_HZ 8000000U

void board_init(void);
void board_led_write(bool on);

#endif
