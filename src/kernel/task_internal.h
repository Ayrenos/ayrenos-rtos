#ifndef AYRENOS_INTERNEL_TASK_H
#define AYRENOS_INTERNEL_TASK_H

#include <stdbool.h>
#include "task.h"

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING
} task_state_t;

typedef struct {
    uint32_t *sp;
    uint32_t *stack_base;
    size_t stack_words;

    uint32_t wake_tick;
    uint8_t priority;
    task_state_t state;
} task_control_t;

task_control_t *task_internal_get(task_id_t id);
size_t task_internal_count(void);

void task_internal_seal_creation(void);

bool task_port_in_handler(void);
uint32_t *task_port_prepare_stack(const task_config_t *config);

#endif /* AYRENOS_INTERNEL_TASK_H */ 