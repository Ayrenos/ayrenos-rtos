#ifndef AYRENOS_SCHEDULER_H
#define AYRENOS_SCHEDULER_H

#include <stdint.h>

#define KERNEL_TICK_HZ 1000U

_Noreturn void kernel_start(void);
uint32_t kernel_ticks(void);

#endif /* AYRENOS_SCHEDULER_H */
