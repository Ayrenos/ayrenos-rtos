#include "kernel/task_internal.h"
#include <stdint.h>
#include "kernel/critical.h"
#include "kernel/debug.h"

_Static_assert(
    sizeof(uintptr_t) == sizeof(uint32_t),
    "This port requires 32-bit addresses"
);

_Static_assert(offsetof(task_control_t, stack_base) == 4, "Unexpected stack base offset");
_Static_assert(offsetof(task_control_t, stack_words) == 8, "Unexpected stack size offset");
_Static_assert(KERNEL_PANIC_STACK == 2, "Assembly requires stack panic code two");

static _Noreturn void task_return_trap(void)
{
    kernel_panic(KERNEL_PANIC_TASK_RETURN);
}

bool task_port_in_handler(void)
{
    uint32_t ipsr;

    __asm volatile (
        "mrs %0, ipsr"
        : "=r" (ipsr)
    );

    return ipsr != 0u;
}

uint32_t *task_port_prepare_stack(const task_config_t *config)
{
    uint32_t *sp = config->stack + config->stack_words;

    sp -= 16;

    for (unsigned i = 0; i < 16u; ++i) {
        sp[i] = 0u;
    }

    sp[8] = (uint32_t)(uintptr_t)config->argument;

    sp[13] = (uint32_t)(uintptr_t)task_return_trap;

    sp[14] = (uint32_t)(uintptr_t)config->entry & ~1u;

    sp[15] = 1u << 24;

    return sp;
}

#define SCB_ICSR   (*(volatile uint32_t *)0xE000ED04U)
#define SCB_SHPR3  (*(volatile uint32_t *)0xE000ED20U)
#define SYST_CSR   (*(volatile uint32_t *)0xE000E010U)
#define SYST_RVR   (*(volatile uint32_t *)0xE000E014U)
#define SYST_CVR   (*(volatile uint32_t *)0xE000E018U)

uint32_t kernel_critical_enter(void)
{
    uint32_t mask;

    __asm volatile (
        "mrs %0, primask\n"
        "cpsid i"
        : "=r" (mask)
        :
        : "memory"
    );

    return mask;
}

void kernel_critical_exit(uint32_t saved_mask)
{
    __asm volatile (
        "msr primask, %0\n"
        "isb"
        :
        : "r" (saved_mask)
        : "memory"
    );
}

bool task_port_can_block(void)
{
    uint32_t primask;
    uint32_t basepri;
    uint32_t faultmask;

    __asm volatile ("mrs %0, primask" : "=r" (primask));
    __asm volatile ("mrs %0, basepri" : "=r" (basepri));
    __asm volatile ("mrs %0, faultmask" : "=r" (faultmask));
    return !task_port_in_handler() && primask == 0U && basepri == 0U && faultmask == 0U;
}

bool task_port_init(uint32_t clock_hz, uint32_t tick_hz)
{
    if (tick_hz == 0U || clock_hz < tick_hz || clock_hz % tick_hz != 0U) {
        return false;
    }

    uint32_t reload = clock_hz / tick_hz - 1U;

    if (reload > 0xFFFFFFU) {
        return false;
    }

    SYST_CSR = 0;
    SYST_RVR = reload;
    SYST_CVR = 0;

    SCB_SHPR3 = (SCB_SHPR3 & 0x0000FFFFU) | (0xFFU << 16) | (0xE0U << 24);
    SCB_ICSR = (1U << 25) | (1U << 27);
    return true;
}

void task_port_start_tick(void)
{
    SYST_CSR = (1U << 2) | (1U << 1) | 1U;
}

void task_port_request_switch(void)
{
    SCB_ICSR = 1U << 28;
    __asm volatile ("dsb\nisb" ::: "memory");
}

void task_port_idle(void)
{
    __asm volatile ("dsb\nwfi" ::: "memory");
}
