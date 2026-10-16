#include "task.h"
#include <stdbool.h>
#include <stdint.h>
#include "task_internal.h"
#include "critical.h"

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

    const uintptr_t id_end = id_begin + sizeof(*out_id);

    if (id_begin > UINTPTR_MAX - sizeof(*out_id) ||
        (begin < id_end && id_begin < end)) {
        return TASK_ERR_ARGUMENT;
    }

    if (used == TASK_MAX_COUNT) {
        return TASK_ERR_CAPACITY;
    }

    for (size_t i = 0; i < used; ++i) {

        const uintptr_t other_begin = (
            uintptr_t)tasks[i].stack_base;

        const uintptr_t other_end = other_begin +
            tasks[i].stack_words * sizeof(uint32_t);

        if (begin < other_end && other_begin < end) {
            return TASK_ERR_STACK_IN_USE;
        }
    }

    task_control_t *task = &tasks[used];

    task_internal_prepare(task, &cfg);

    *out_id = (task_id_t)used;
    used++;

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

void task_internal_seal_creation(void)
{
    creation_sealed = true;
}

void task_internal_prepare(task_control_t *task, const task_config_t *config)
{
    for (size_t i = 0; i < config->stack_words; ++i) {
        config->stack[i] = TASK_STACK_FILL;
    }

    config->stack[0] = TASK_STACK_CANARY;
    task->sp = task_port_prepare_stack(config);
    task->stack_base = config->stack;
    task->stack_words = config->stack_words;
    task->wake_tick = 0;
    task->priority = config->priority;
    task->base_priority = config->priority;
    task->state = TASK_READY;
    task->wait_kind = TASK_WAIT_NONE;
    task->wait_object = NULL;
    task->wait_buffer = NULL;
    task->wait_forever = false;
    task->wait_result = TASK_OK;
    task->wait_order = 0;
}

task_result_t task_stack_usage(task_id_t id, size_t *out_words)
{
    if (out_words == NULL) {
        return TASK_ERR_ARGUMENT;
    }

    uint32_t mask = kernel_critical_enter();
    task_control_t *task = task_internal_get(id);

    if (task == NULL) {
        kernel_critical_exit(mask);
        return TASK_ERR_ARGUMENT;
    }

    if (task->stack_base[0] != TASK_STACK_CANARY) {
        kernel_critical_exit(mask);
        return TASK_ERR_STATE;
    }

    size_t unused = 1;

    while (unused < task->stack_words &&
           task->stack_base[unused] == TASK_STACK_FILL) {
        ++unused;
    }

    *out_words = task->stack_words - unused;
    kernel_critical_exit(mask);
    return TASK_OK;
}
