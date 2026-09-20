#include "kernel/task.h"

_Alignas(8) static uint32_t worker_stack[256];

static task_id_t worker_id;

volatile task_result_t boot_task_result;

static void worker(void *argument)
{
    (void)argument;

    for (;;) {
        __asm volatile ("wfi");
    }
}

int main(void)
{
    const task_config_t config = {
        .entry = worker,
        .argument = NULL,
        .stack = worker_stack,
        .stack_words = sizeof(worker_stack) / sizeof(worker_stack[0]),
        .priority = 2
    };

    boot_task_result = task_create(&config, &worker_id);

    if (boot_task_result != TASK_OK) {
        for (;;) {
            __asm volatile ("wfi");
        }
    }

    for (;;) {
        __asm volatile ("wfi");
    }
}