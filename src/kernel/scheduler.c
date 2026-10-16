#include "scheduler.h"
#include "task_internal.h"
#include "critical.h"
#include "debug.h"
#include "board.h"
#include <stddef.h>

_Static_assert(
    offsetof(task_control_t, sp) == 0,
    "Assembly requires sp as the first TCB member"
);

task_control_t *scheduler_current;

static uint32_t tick_count;
static uint32_t wait_sequence;
static task_control_t idle_task;
_Alignas(8) static uint32_t idle_stack[128];

static void idle(void *argument)
{
    (void)argument;

    for (;;) {
        task_port_idle();
    }
}

/* Unsigned subtraction handles tick wrap without signed overflow. */
static bool tick_reached(uint32_t now, uint32_t deadline)
{
    return (uint32_t)(now - deadline) <= (uint32_t)TASK_WAIT_MAX;
}

static void check_stack(task_control_t *task)
{
    uintptr_t sp = (uintptr_t)task->sp;
    uintptr_t begin = (uintptr_t)task->stack_base;
    uintptr_t end = begin + task->stack_words * sizeof(uint32_t);

    if (task->stack_base[0] != TASK_STACK_CANARY ||
        sp < begin + sizeof(uint32_t) || sp > end - 16U * sizeof(uint32_t) ||
        (sp & 7U) != 0U) {
        kernel_panic(KERNEL_PANIC_STACK);
    }
}

/* Called by PendSV after it saves the outgoing task's context. */
void scheduler_select(void)
{
    if (scheduler_current != NULL) {
        check_stack(scheduler_current);

        if (scheduler_current->state == TASK_RUNNING) {
            scheduler_current->state = TASK_READY;
        }
    }

    sync_internal_refresh_priorities();
    size_t count = task_internal_count();
    size_t start = 0;

    for (size_t i = 0; i < count; ++i) {
        if (task_internal_get((task_id_t)i) == scheduler_current) {
            start = (i + 1U) % count;
            break;
        }
    }

    task_control_t *next = &idle_task;

    for (size_t i = 0; i < count; ++i) {
        task_control_t *task = task_internal_get((task_id_t)((start + i) % count));

        if (task->state == TASK_READY && task->priority > next->priority) {
            next = task;
        }
    }

    check_stack(next);
    scheduler_current = next;
    next->state = TASK_RUNNING;
}

void scheduler_wake(task_control_t *task, task_result_t result)
{
    task->wait_kind = TASK_WAIT_NONE;
    task->wait_object = NULL;
    task->wait_buffer = NULL;
    task->wait_result = result;
    task->state = TASK_READY;
    task_port_request_switch();
}

void scheduler_tick(void)
{
    ++tick_count;

    for (size_t i = 0; i < task_internal_count(); ++i) {
        task_control_t *task = task_internal_get((task_id_t)i);

        if (task->state == TASK_SLEEPING && tick_reached(tick_count, task->wake_tick)) {
            scheduler_wake(task, TASK_OK);
        } else if (task->state == TASK_BLOCKED && !task->wait_forever &&
                   tick_reached(tick_count, task->wake_tick)) {
            scheduler_wake(task, TASK_ERR_TIMEOUT);
        }
    }

    sync_internal_refresh_priorities();
    /* One tick per time slice; priority selection happens in PendSV. */
    task_port_request_switch();
}

void SysTick_Handler(void)
{
    uint32_t mask = kernel_critical_enter();
    scheduler_tick();
    kernel_critical_exit(mask);
}

void scheduler_block(void *object, task_wait_t kind, void *buffer, uint32_t ticks)
{
    scheduler_current->wait_object = object;
    scheduler_current->wait_kind = kind;
    scheduler_current->wait_buffer = buffer;
    scheduler_current->wait_forever = ticks == TASK_WAIT_FOREVER;
    scheduler_current->wake_tick = tick_count + ticks;
    scheduler_current->wait_result = TASK_ERR_STATE;
    scheduler_current->wait_order = wait_sequence++;
    scheduler_current->state = TASK_BLOCKED;
    sync_internal_refresh_priorities();
    task_port_request_switch();
}

uint32_t kernel_ticks(void)
{
    uint32_t mask = kernel_critical_enter();
    uint32_t ticks = tick_count;
    kernel_critical_exit(mask);
    return ticks;
}

task_result_t task_yield(void)
{
    if (!task_port_can_block()) {
        return TASK_ERR_CONTEXT;
    }

    uint32_t mask = kernel_critical_enter();

    if (scheduler_current == NULL) {
        kernel_critical_exit(mask);
        return TASK_ERR_STATE;
    }

    task_port_request_switch();
    kernel_critical_exit(mask);
    return TASK_OK;
}

task_result_t task_sleep(uint32_t ticks)
{
    if (ticks == 0U) {
        return task_yield();
    }

    if (ticks > (uint32_t)TASK_WAIT_MAX) {
        return TASK_ERR_ARGUMENT;
    }

    if (!task_port_can_block()) {
        return TASK_ERR_CONTEXT;
    }

    uint32_t mask = kernel_critical_enter();

    if (scheduler_current == NULL) {
        kernel_critical_exit(mask);
        return TASK_ERR_STATE;
    }

    scheduler_current->wake_tick = tick_count + ticks;
    scheduler_current->state = TASK_SLEEPING;
    task_port_request_switch();
    kernel_critical_exit(mask);
    return TASK_OK;
}

_Noreturn void kernel_start(void)
{
    if (!task_port_can_block() || scheduler_current != NULL) {
        kernel_panic(KERNEL_PANIC_STARTUP);
    }

    uint32_t mask = kernel_critical_enter();
    const task_config_t config = {
        .entry = idle,
        .argument = NULL,
        .stack = idle_stack,
        .stack_words = sizeof(idle_stack) / sizeof(idle_stack[0]),
        .priority = 0
    };

    task_internal_prepare(&idle_task, &config);
    task_internal_seal_creation();

    if (!task_port_init(BOARD_CORE_CLOCK_HZ, KERNEL_TICK_HZ)) {
        kernel_panic(KERNEL_PANIC_STARTUP);
    }

    scheduler_select();
    kernel_critical_exit(mask);
    task_port_start_first();
}
