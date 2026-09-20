#include "task.h"
#include <stdbool.h>
#include <stdint.h>
#include "task_internal.h"

static task_control_t tasks[TASK_MAX_COUNT];
static size_t used;
static bool creation_sealed;

_Static_assert(
    TASK_MAX_COUNT <= UINT16_MAX,
    "Task ID Type Too Small"
);

task_result_t task_create(const task_config_t *config, task_id_t *out_id)
{
    if (config == NULL || out_id == NULL) {
        return TASK_ERR_ARGUMENT;
    }

    if (task_port_in_handler()) {
        return TASK_ERR_CONTEXT;
    }

    if (creation_sealed) {
        return TASK_ERR_KERNEL_STARTED;
    }

    const task_config_t cfg = *config;

    if (cfg.entry == NULL || cfg.stack == NULL) {
        return TASK_ERR_ARGUMENT;
    }

    if (cfg.priority < TASK_PRIORITY_MIN || 
        cfg.priority > TASK_PRIORITY_MAX) {

        return TASK_ERR_PRIORITY;
    }

    const uintptr_t begin = (uintptr_t)cfg.stack;

    if ((begin & 7U) != 0U) {
        return TASK_ERR_STACK_ALIGNMENT;
    }

    if (cfg.stack_words < TASK_MIN_STACK_WORDS ||
       (cfg.stack_words & 1U) != 0U ||
        cfg.stack_words > (UINTPTR_MAX - begin) / sizeof(uint32_t)) {

        return TASK_ERR_STACK_SIZE;
    }

    const uintptr_t end = begin + cfg.stack_words * sizeof(uint32_t);

    const uintptr_t id_begin = (uintptr_t)out_id;

    if (id_begin > UINTPTR_MAX - sizeof(*out_id) ||
       (begin < end && begin < id_begin + sizeof(*out_id))) {

        return TASK_ERR_ARGUMENT;
    }

    if (used == TASK_MAX_COUNT) {
        return TASK_ERR_CAPACITY;
    }

    for (size_t i = 0; i < used; ++i) {

        const uintptr_t other_begin = other_begin +
            (uintptr_t)tasks[i].stack_base;

        const uintptr_t other_end = other_begin +
            tasks[i].stack_words * sizeof(uint32_t);

        if (begin < other_end && other_begin < end) {
            return TASK_ERR_STACK_IN_USE;
        }
    }

    task_control_t *task = &tasks[used];

    task->sp = task_port_prepare_stack(&cfg);
    task->stack_base = cfg.stack;
    task->stack_words = cfg.stack_words;
    task->wake_tick = 0;
    task->priority = cfg.priority;
    task->state = TASK_READY;

    *out_id = (task_id_t)used;

    return TASK_OK;
}

task_control_t *task_internal_get(task_id_t id)
{
    return (size_t)id < used ? &tasks[id] : NULL;
}

size_t task_internal_count(void)
{
    return used;
}

void task_internal_deal_creation(void)
{
    creation_sealed = true;
}