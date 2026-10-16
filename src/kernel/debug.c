#include "debug.h"
#include "critical.h"
#include <stddef.h>

volatile kernel_panic_t kernel_panic_reason;
volatile kernel_fault_t kernel_fault;

extern uint32_t _sram;
extern uint32_t _eram;

_Noreturn void kernel_panic(kernel_panic_t reason)
{
    (void)kernel_critical_enter();
    kernel_panic_reason = reason;

    /* Set a debugger breakpoint here to inspect the failure. */
    for (;;) {
        __asm volatile ("nop");
    }
}

_Noreturn void kernel_hardfault(const uint32_t *frame, uint32_t exception_return)
{
    (void)kernel_critical_enter();
    kernel_fault.exception_return = exception_return;
    kernel_fault.cfsr = *(volatile uint32_t *)0xE000ED28U;
    kernel_fault.hfsr = *(volatile uint32_t *)0xE000ED2CU;
    kernel_fault.mmfar = *(volatile uint32_t *)0xE000ED34U;
    kernel_fault.bfar = *(volatile uint32_t *)0xE000ED38U;
    kernel_fault.frame_valid = 0;

    uintptr_t address = (uintptr_t)frame;
    uintptr_t begin = (uintptr_t)&_sram;
    uintptr_t end = (uintptr_t)&_eram;

    /* Stacking faults can leave an incomplete frame; never dereference it. */
    if ((kernel_fault.cfsr & ((1U << 4) | (1U << 12))) == 0U &&
        (address & 3U) == 0U && address >= begin && address <= end - 32U) {
        for (size_t i = 0; i < 8U; ++i) {
            kernel_fault.frame[i] = frame[i];
        }

        kernel_fault.frame_valid = 1;
    }

    kernel_panic(KERNEL_PANIC_HARDFAULT);
}
