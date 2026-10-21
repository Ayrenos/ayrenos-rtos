#ifndef AYRENOS_DEBUG_H
#define AYRENOS_DEBUG_H

#include <stdint.h>

typedef enum {
    KERNEL_PANIC_NONE = 0,
    KERNEL_PANIC_STARTUP,
    KERNEL_PANIC_STACK,
    KERNEL_PANIC_TASK_RETURN,
    KERNEL_PANIC_HARDFAULT
} kernel_panic_t;

typedef struct {
    uint32_t exception_return;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t mmfar;
    uint32_t bfar;
    uint32_t frame_valid;
    uint32_t frame[8];
} kernel_fault_t;

extern volatile kernel_panic_t kernel_panic_reason;
extern volatile kernel_fault_t kernel_fault;

_Noreturn void kernel_panic(kernel_panic_t reason);
_Noreturn void kernel_hardfault(const uint32_t *frame, uint32_t exception_return);

#endif
