#ifndef AYRENOS_CRITICAL_H
#define AYRENOS_CRITICAL_H

#include <stdint.h>

uint32_t kernel_critical_enter(void);
void kernel_critical_exit(uint32_t saved_mask);

#endif
