#ifndef AYRENOS_CRITICAL_H
#define AYRENOS_CRITICAL_H

#include <stdint.h>

/* Save and restore PRIMASK so nested sections preserve interrupt state. */
uint32_t kernel_critical_enter(void);
void kernel_critical_exit(uint32_t saved_mask);

#endif /* AYRENOS_CRITICAL_H */
